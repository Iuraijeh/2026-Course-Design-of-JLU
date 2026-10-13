const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

const root = path.resolve(process.env.MICROX_DATA_DIR || path.join(__dirname, '..'));
const userFile = path.join(root, 'users.txt');
const groupFile = path.join(root, 'groups.txt');
const messageFile = path.join(root, 'messages.txt');
const services = ['QQ', 'WeChat', 'Weibo'];

function lines(file) {
  try { return fs.readFileSync(file, 'utf8').replace(/^\uFEFF/, '').split(/\r?\n/).filter(Boolean); }
  catch (error) { if (error.code === 'ENOENT') return []; throw error; }
}

function list(text) { return text ? text.split(',').filter(Boolean) : []; }
function unique(items) { return [...new Set(items)]; }
function clean(value, max = 200) {
  if (typeof value !== 'string') throw new Error('输入内容无效');
  const result = value.trim();
  if (!result || result.length > max || /[|\r\n]/.test(result)) throw new Error('输入内容为空、过长或含有非法分隔符');
  return result;
}
function account(value) {
  const result = clean(value, 64).toLowerCase();
  if (!/^[a-z0-9]+$/.test(result)) throw new Error('账号只能包含字母和数字');
  return result;
}
function wechatId(value) {
  const result = clean(value, 20);
  if (!/^[A-Za-z0-9_]{6,20}$/.test(result))
    throw new Error('微信账号号须为 6–20 位字母、数字或下划线');
  return result;
}
function wechatIdTaken(users, value, exceptAccount = '') {
  const wanted = value.toLowerCase();
  return users.some(u => u.account.toLowerCase() === wanted ||
    (u.account.toLowerCase() !== exceptAccount.toLowerCase() &&
      (u.wechatId || '').toLowerCase() === wanted));
}
function generatedWechatId(accountNumber, salt = 0) {
  const input = salt ? `${accountNumber}#${salt}` : accountNumber;
  let hash = 14695981039346656037n;
  for (const byte of Buffer.from(input, 'utf8'))
    hash = ((hash ^ BigInt(byte)) * 1099511628211n) & 0xffffffffffffffffn;
  return `wx_${hash.toString(16).padStart(16, '0')}`;
}
function ensureWechatIds(users) {
  const occupied = new Set(users.flatMap(u => [u.account, u.wechatId || ''])
    .filter(Boolean).map(x => x.toLowerCase()));
  for (const user of users) {
    if (!user.services.includes('WeChat') || user.wechatId) continue;
    let salt = 0, candidate;
    do { candidate = generatedWechatId(user.account, salt++); }
    while (occupied.has(candidate.toLowerCase()));
    user.wechatId = candidate;
    occupied.add(candidate.toLowerCase());
  }
  return users;
}
function atomicWrite(file, content) {
  const temp = `${file}.${process.pid}.${crypto.randomBytes(4).toString('hex')}.tmp`;
  fs.writeFileSync(temp, content, 'utf8');
  try { fs.renameSync(temp, file); }
  catch (error) { try { fs.unlinkSync(temp); } catch {} throw error; }
}

function readUsers() {
  return ensureWechatIds(lines(userFile).map(line => {
    const f = line.split('|');
    if (f.length < 5) return null;
    return {
      account: f[0], id: f[1], birthday: f[2], registered: f[3], location: f[4],
      services: list(f[5]), friendIds: list(f[6]), groupIds: list(f[7]), password: f[8] || f[0],
      wechatId: f[9] || '', wechatIdChangedMonth: f[10] || '', wechatBoundQQ: f[11] || ''
    };
  }).filter(Boolean));
}
function writeUsers(users) {
  ensureWechatIds(users);
  atomicWrite(userFile, users.map(u => [u.account, u.id, u.birthday, u.registered,
    u.location, unique(u.services).join(','), unique(u.friendIds).join(','),
    unique(u.groupIds).join(','), u.password, u.wechatId || '',
    u.wechatIdChangedMonth || '', u.wechatBoundQQ || ''].join('|')).join('\n') + (users.length ? '\n' : ''));
}
function findUser(users, value) { return users.find(u => u.account.toLowerCase() === String(value).toLowerCase()); }

function friendFile(userAccount) { return path.join(root, `friends_${account(userAccount)}.txt`); }
function readFriends(userAccount) {
  return lines(friendFile(userAccount)).map(line => {
    const f = line.split('|');
    return f.length >= 3 ? { account: f[0], remark: f[1], services: list(f[2]) } : null;
  }).filter(Boolean);
}
function writeFriends(userAccount, friends) {
  atomicWrite(friendFile(userAccount), friends.map(f => [f.account, f.remark,
    unique(f.services).join(',')].join('|')).join('\n') + (friends.length ? '\n' : ''));
}

function readGroups() {
  return lines(groupFile).map(line => {
    const f = line.split('|');
    if (f.length < 4) return null;
    const subgroups = (f[6] || '').split(';').filter(Boolean).map(entry => {
      const colon = entry.indexOf(':');
      return { name: entry.slice(0, colon), members: list(entry.slice(colon + 1)) };
    });
    return { id: f[0], name: f[1], owner: f[2], strategy: f[3],
      members: list(f[4]), admins: list(f[5]), subgroups };
  }).filter(Boolean);
}
function writeGroups(groups) {
  atomicWrite(groupFile, groups.map(g => [g.id, g.name, g.owner, g.strategy,
    unique(g.members).join(','), unique(g.admins).join(','),
    g.subgroups.map(s => `${s.name}:${unique(s.members).join(',')}`).join(';')]
    .join('|')).join('\n') + (groups.length ? '\n' : ''));
}

function readMessages() {
  return lines(messageFile).map(line => {
    const f = line.split('|');
    if (f.length !== 7) return null;
    try { return { id: f[0], at: Number(f[1]), kind: f[2], sender: f[3],
      target: f[4], text: Buffer.from(f[5], 'hex').toString('utf8'), transport: f[6] }; }
    catch { return null; }
  }).filter(Boolean);
}
function appendMessage({ id = crypto.randomUUID(), at = Date.now(), kind, sender, target, text, transport = 'web' }) {
  if (!['direct', 'group'].includes(kind)) throw new Error('消息类型无效');
  sender = account(sender);
  target = kind === 'direct' ? account(target) : clean(target, 64);
  text = clean(text, 4000);
  if (!/^[a-zA-Z0-9-]{8,80}$/.test(id) || !Number.isSafeInteger(at) || at <= 0) throw new Error('消息编号或时间无效');
  if (readMessages().some(message => message.id === id)) return false;
  fs.appendFileSync(messageFile, [id, at, kind, sender, target,
    Buffer.from(text, 'utf8').toString('hex'), transport].join('|') + '\n', 'utf8');
  return true;
}

module.exports = { root, services, clean, account, wechatId, wechatIdTaken,
  ensureWechatIds, readUsers, writeUsers, findUser,
  readFriends, writeFriends, readGroups, writeGroups, readMessages, appendMessage };
