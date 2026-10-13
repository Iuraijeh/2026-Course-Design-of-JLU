const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const crypto = require('node:crypto');
const db = require('./data');

const sessions = new Map();
const publicDir = path.join(__dirname, 'public');
const WEB_PORT = Number(process.env.MICROX_WEB_PORT || 3000);
const TCP_PORT = Number(process.env.MICROX_TCP_PORT || 9527);
let tcpListening = false;

function fail(message, status = 400) { const error = new Error(message); error.status = status; throw error; }
function json(response, status, data, extraHeaders = {}) {
  response.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store', ...extraHeaders });
  response.end(JSON.stringify(data));
}
function currentAccount(request) {
  const cookies = Object.fromEntries((request.headers.cookie || '').split(';').map(part => {
    const i = part.indexOf('='); return i < 0 ? ['', ''] : [part.slice(0, i).trim(), part.slice(i + 1).trim()];
  }));
  return cookies.sid ? sessions.get(cookies.sid) : null;
}
function sessionCookie(token, maxAge = 86400) {
  return `sid=${token}; HttpOnly; SameSite=Strict; Path=/; Max-Age=${maxAge}`;
}
async function body(request) {
  let raw = '';
  for await (const chunk of request) {
    raw += chunk;
    if (raw.length > 64 * 1024) fail('请求内容过长', 413);
  }
  try { return JSON.parse(raw || '{}'); } catch { fail('JSON 格式错误'); }
}
function requireUser(request) {
  const account = currentAccount(request);
  const users = db.readUsers();
  const user = account && db.findUser(users, account);
  if (!user) fail('请先登录', 401);
  return { users, user };
}
function saveUser(users) { db.writeUsers(users); }
function service(value) {
  if (!db.services.includes(value)) fail('服务类型无效');
  return value;
}
function currentMonth() {
  const parts = new Intl.DateTimeFormat('en-US', { timeZone: 'Asia/Shanghai',
    year: 'numeric', month: '2-digit' }).formatToParts(new Date());
  const part = type => parts.find(item => item.type === type).value;
  return `${part('year')}-${part('month')}`;
}
function publicUser(user) {
  return { account: user.account, id: user.id, nickname: user.id, birthday: user.birthday,
    registered: user.registered, location: user.location, services: user.services,
    friendIds: user.friendIds, groupIds: user.groupIds, wechatId: user.wechatId,
    wechatIdChangedMonth: user.wechatIdChangedMonth, wechatBoundQQ: user.wechatBoundQQ };
}
function visibleMessages(account, groups) {
  const joined = new Set(groups.filter(g => g.members.includes(account)).map(g => g.id));
  return db.readMessages().filter(m => m.kind === 'direct'
    ? m.sender === account || m.target === account
    : joined.has(m.target));
}
function state(request) {
  const { users, user } = requireUser(request);
  const groups = db.readGroups();
  return { user: publicUser(user), users: users.map(u => ({ account: u.account,
    id: u.id, nickname: u.id, wechatId: u.wechatId })),
    friends: db.readFriends(user.account), groups,
    messages: visibleMessages(user.account, groups), tcp: { listening: tcpListening, port: TCP_PORT } };
}
function assertMember(group, account) {
  if (!group.members.includes(account)) fail('只有群成员可以执行此操作', 403);
}
function groupAction(data, users, user) {
  const groups = db.readGroups();
  const group = groups.find(g => g.id === String(data.groupId));
  if (!group) fail('群不存在', 404);
  const actor = user.account;
  const target = data.target ? db.account(data.target) : '';
  let targetUser = target && db.findUser(users, target);
  switch (data.action) {
    case 'join':
      if (group.strategy !== 'QQ') fail('微信群模式需要成员邀请');
      if (group.members.includes(actor)) fail('你已经在群内');
      group.members.push(actor); user.groupIds.push(group.id); break;
    case 'invite':
      assertMember(group, actor);
      if (!targetUser) fail('被邀请账号不存在');
      if (group.members.includes(target)) fail('对方已在群内');
      group.members.push(target); targetUser.groupIds.push(group.id); break;
    case 'quit':
      assertMember(group, actor);
      if (group.owner === actor) fail('群主不能退群');
      group.members = group.members.filter(x => x !== actor);
      group.admins = group.admins.filter(x => x !== actor);
      group.subgroups.forEach(s => { s.members = s.members.filter(x => x !== actor); });
      user.groupIds = user.groupIds.filter(x => x !== group.id); break;
    case 'kick':
      assertMember(group, actor);
      if (!group.members.includes(target)) fail('该用户不在群内');
      if (target === group.owner || actor !== group.owner &&
          !(group.strategy === 'QQ' && group.admins.includes(actor) && !group.admins.includes(target)))
        fail('没有移除该成员的权限', 403);
      group.members = group.members.filter(x => x !== target);
      group.admins = group.admins.filter(x => x !== target);
      group.subgroups.forEach(s => { s.members = s.members.filter(x => x !== target); });
      if (targetUser) targetUser.groupIds = targetUser.groupIds.filter(x => x !== group.id);
      break;
    case 'admin.add':
    case 'admin.remove':
      if (group.owner !== actor) fail('只有群主可以管理管理员', 403);
      if (group.strategy !== 'QQ') fail('当前模式没有管理员制度');
      if (!group.members.includes(target) || target === group.owner) fail('目标不是普通群成员');
      if (data.action === 'admin.add') group.admins = [...new Set([...group.admins, target])];
      else group.admins = group.admins.filter(x => x !== target);
      break;
    case 'subgroup': {
      assertMember(group, actor);
      if (group.strategy !== 'QQ') fail('当前模式不支持讨论组');
      const name = db.clean(data.name, 60);
      if (group.subgroups.some(s => s.name === name)) fail('讨论组已存在');
      group.subgroups.push({ name, members: [actor] }); break;
    }
    case 'strategy':
      assertMember(group, actor);
      if (!['QQ', 'WeChat'].includes(data.strategy)) fail('群模式无效');
      group.strategy = data.strategy; break;
    default: fail('未知群操作');
  }
  db.writeGroups(groups);
  saveUser(users);
}

function doAction(data, request) {
  const { users, user } = requireUser(request);
  const me = user.account;
  switch (data.type) {
    case 'profile.id':
    case 'profile.nickname':
      user.id = db.clean(data.nickname ?? data.id, 80); saveUser(users); break;
    case 'profile.password':
      if (user.password !== data.oldPassword) fail('原密码错误');
      if (typeof data.password !== 'string' || data.password.length < 6 || data.password.length > 12)
        fail('新密码须为 6–12 位');
      user.password = data.password; saveUser(users); break;
    case 'service.open':
    case 'service.close': {
      const selected = service(data.service);
      user.services = data.type.endsWith('open')
        ? [...new Set([...user.services, selected])]
        : user.services.filter(x => x !== selected);
      saveUser(users); break;
    }
    case 'wechat.rename': {
      if (!user.services.includes('WeChat')) fail('请先开通微信服务');
      const next = db.wechatId(data.wechatId);
      if (next === user.wechatId) break;
      if (user.wechatIdChangedMonth === currentMonth()) fail('本自然月已修改过微信账号号');
      if (db.wechatIdTaken(users, next, me)) fail('微信账号号已被使用');
      user.wechatId = next;
      user.wechatIdChangedMonth = currentMonth();
      saveUser(users); break;
    }
    case 'wechat.bind':
      if (!user.services.includes('WeChat')) fail('请先开通微信服务');
      if (!user.services.includes('QQ')) fail('请先登录 QQ 服务');
      user.wechatBoundQQ = me;
      saveUser(users); break;
    case 'wechat.unbind':
      user.wechatBoundQQ = '';
      saveUser(users); break;
    case 'friend.add': {
      const selected = service(data.service);
      if (!user.services.includes(selected)) fail('请先开通该服务');
      let targetUser;
      if (selected === 'WeChat') {
        const identifier = db.clean(data.target, 64).toLowerCase();
        if (!/^[a-z0-9]{1,64}$/.test(identifier) &&
            !/^[a-z0-9_]{6,20}$/.test(identifier)) fail('微信账号号格式无效');
        targetUser = users.find(u => u.services.includes('WeChat') &&
          ((u.wechatId || '').toLowerCase() === identifier ||
            (u.wechatBoundQQ || '').toLowerCase() === identifier));
      } else targetUser = db.findUser(users, db.account(data.target));
      if (!targetUser || targetUser.account === me) fail('好友账号不存在');
      const target = targetUser.account;
      const friends = db.readFriends(me);
      const existing = friends.find(f => f.account.toLowerCase() === target);
      if (existing) {
        existing.services = [...new Set([...existing.services, selected])];
        if (data.remark) existing.remark = db.clean(data.remark, 80);
      } else friends.push({ account: target, remark: db.clean(data.remark || target, 80), services: [selected] });
      user.friendIds = [...new Set([...user.friendIds, target])];
      db.writeFriends(me, friends); saveUser(users); break;
    }
    case 'friend.update': {
      const friends = db.readFriends(me);
      const friend = friends.find(f => f.account === db.account(data.target));
      if (!friend) fail('好友不存在');
      friend.remark = db.clean(data.remark, 80);
      friend.services = [service(data.service)];
      db.writeFriends(me, friends); break;
    }
    case 'friend.remove': {
      const target = db.account(data.target);
      const friends = db.readFriends(me);
      if (!friends.some(f => f.account === target)) fail('好友不存在');
      db.writeFriends(me, friends.filter(f => f.account !== target));
      user.friendIds = user.friendIds.filter(x => x !== target); saveUser(users); break;
    }
    case 'friend.recommend': {
      const from = service(data.from), to = service(data.to), target = db.account(data.target);
      if (!user.services.includes(from) || !user.services.includes(to)) fail('请先开通这两个服务');
      const friends = db.readFriends(me);
      const friend = friends.find(f => f.account === target);
      if (!friend || !friend.services.includes(from)) fail('来源服务中没有该好友');
      friend.services = [...new Set([...friend.services, to])];
      db.writeFriends(me, friends); break;
    }
    case 'group': groupAction(data, users, user); break;
    case 'message.send': {
      const text = db.clean(data.text, 4000);
      if (data.kind === 'direct') {
        const target = db.account(data.target);
        if (!db.findUser(users, target)) fail('收件账号不存在');
        if (!db.readFriends(me).some(f => f.account === target)) fail('请先添加对方为好友');
        db.appendMessage({ kind: 'direct', sender: me, target, text });
      } else if (data.kind === 'group') {
        const group = db.readGroups().find(g => g.id === String(data.target));
        if (!group) fail('群不存在');
        assertMember(group, me);
        db.appendMessage({ kind: 'group', sender: me, target: group.id, text });
      } else fail('消息类型无效');
      break;
    }
    default: fail('未知操作');
  }
  return { ok: true };
}

function sendTcp(data, request) {
  const { users, user } = requireUser(request);
  const target = db.account(data.target);
  if (!db.findUser(users, target)) fail('收件账号不存在');
  const ip = String(data.ip || '');
  const port = Number(data.port);
  if (!net.isIP(ip) || !Number.isInteger(port) || port < 1 || port > 65535) fail('IP 或端口无效');
  const text = db.clean(data.text, 4000);
  const id = crypto.randomUUID(), at = Date.now();
  const payload = `MX1|${id}|${at}|${user.account}|${target}|${Buffer.from(text).toString('hex')}\n`;
  return new Promise((resolve, reject) => {
    const socket = net.createConnection({ host: ip, port, timeout: 5000 }, () => socket.end(payload));
    socket.on('close', hadError => {
      if (hadError) return;
      try { db.appendMessage({ id, at, kind: 'direct', sender: user.account,
        target, text, transport: 'tcp' }); resolve({ ok: true }); }
      catch (error) { reject(error); }
    });
    socket.on('timeout', () => socket.destroy(new Error('连接超时')));
    socket.on('error', error => reject(new Error(`TCP 发送失败：${error.message}`)));
  });
}

function staticFile(request, response) {
  const url = new URL(request.url, 'http://localhost');
  const allowed = { '/': 'index.html', '/app.js': 'app.js', '/style.css': 'style.css' };
  const name = allowed[url.pathname];
  if (!name) return json(response, 404, { error: '页面不存在' });
  const mime = name.endsWith('.js') ? 'text/javascript' : name.endsWith('.css') ? 'text/css' : 'text/html';
  response.writeHead(200, { 'Content-Type': `${mime}; charset=utf-8`,
    'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff' });
  fs.createReadStream(path.join(publicDir, name)).pipe(response);
}

const server = http.createServer(async (request, response) => {
  try {
    const url = new URL(request.url, 'http://localhost');
    if (!url.pathname.startsWith('/api/')) return staticFile(request, response);
    if (request.method === 'POST') {
      const origin = request.headers.origin;
      if (origin && !/^http:\/\/(127\.0\.0\.1|localhost)(:\d+)?$/.test(origin)) fail('请求来源无效', 403);
    }
    if (url.pathname === '/api/state' && request.method === 'GET') return json(response, 200, state(request));
    if (url.pathname === '/api/login' && request.method === 'POST') {
      const data = await body(request), users = db.readUsers();
      const user = db.findUser(users, db.account(data.account));
      if (!user || user.password !== data.password) fail('账号或密码错误', 401);
      const token = crypto.randomBytes(32).toString('hex');
      sessions.set(token, user.account);
      return json(response, 200, { ok: true }, { 'Set-Cookie': sessionCookie(token) });
    }
    if (url.pathname === '/api/register' && request.method === 'POST') {
      const data = await body(request), users = db.readUsers();
      const account = db.account(data.account), id = db.clean(data.id, 80);
      if (db.wechatIdTaken(users, account)) fail('账号号已被使用');
      if (typeof data.password !== 'string' || data.password.length < 6 || data.password.length > 12)
        fail('密码须为 6–12 位');
      if (data.password !== data.confirm) fail('两次密码不一致');
      users.push({ account, id, birthday: '1970-01-01',
        registered: new Date().toISOString().slice(0, 10), location: '',
        services: [...db.services], friendIds: [], groupIds: [], password: data.password });
      db.writeUsers(users);
      return json(response, 200, { ok: true });
    }
    if (url.pathname === '/api/logout' && request.method === 'POST') {
      const token = (request.headers.cookie || '').match(/(?:^|;\s*)sid=([^;]+)/)?.[1];
      if (token) sessions.delete(token);
      return json(response, 200, { ok: true }, { 'Set-Cookie': sessionCookie('', 0) });
    }
    if (url.pathname === '/api/action' && request.method === 'POST')
      return json(response, 200, doAction(await body(request), request));
    if (url.pathname === '/api/tcp' && request.method === 'POST')
      return json(response, 200, await sendTcp(await body(request), request));
    return json(response, 404, { error: '接口不存在' });
  } catch (error) {
    json(response, error.status || 400, { error: error.message || '请求失败' });
  }
});

const tcpServer = net.createServer(socket => {
  let payload = '';
  socket.setTimeout(5000);
  socket.on('data', chunk => { payload += chunk.toString('utf8'); if (payload.length > 20000) socket.destroy(); });
  socket.on('end', () => {
    const parts = payload.trim().split('|');
    if (parts.length !== 6 || parts[0] !== 'MX1') return;
    try {
      const users = db.readUsers();
      if (!db.findUser(users, parts[3]) || !db.findUser(users, parts[4])) return;
      db.appendMessage({ id: parts[1], at: Number(parts[2]), kind: 'direct',
        sender: parts[3], target: parts[4], text: Buffer.from(parts[5], 'hex').toString('utf8'), transport: 'tcp' });
    } catch (error) { console.error('[TCP]', error.message); }
  });
  socket.on('timeout', () => socket.destroy());
});
tcpServer.on('listening', () => { tcpListening = true; });
tcpServer.on('error', error => {
  tcpListening = false;
  console.warn(`[TCP] ${TCP_PORT} 未能监听：${error.message}。网页仍可发送到已有的控制台接收器。`);
});

if (require.main === module) {
  server.listen(WEB_PORT, '127.0.0.1', () =>
    console.log(`微X 网页已启动：http://127.0.0.1:${WEB_PORT}`));
  tcpServer.listen(TCP_PORT, '0.0.0.0');
}
module.exports = { server, tcpServer };
