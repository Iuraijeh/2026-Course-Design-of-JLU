const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'microx-test-'));
process.env.MICROX_DATA_DIR = temp;
fs.writeFileSync(path.join(temp, 'users.txt'), [
  '10001|张三|2000-01-01|2020-01-01||QQ,WeChat|10002|1001|123456',
  '10002|李四|2000-01-01|2020-01-01||QQ|10001|1001|123456'
].join('\n') + '\n');
fs.writeFileSync(path.join(temp, 'friends_10001.txt'), '10002|李四|QQ\n');
fs.writeFileSync(path.join(temp, 'friends_10002.txt'), '10001|张三|QQ\n');
fs.writeFileSync(path.join(temp, 'groups.txt'), '1001|测试群|10001|QQ|10001,10002||\n');
const { server, tcpServer } = require('./server');

test('账号、好友、群聊和 TCP 消息在双方视图中持久化', async () => {
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  await new Promise(resolve => tcpServer.listen(0, '127.0.0.1', resolve));
  const base = `http://127.0.0.1:${server.address().port}`;
  async function call(url, payload, cookie = '') {
    const response = await fetch(base + url, {
      method: payload === undefined ? 'GET' : 'POST',
      headers: { 'Content-Type': 'application/json', cookie },
      body: payload === undefined ? undefined : JSON.stringify(payload)
    });
    return { status: response.status, data: await response.json(), cookie: response.headers.get('set-cookie')?.split(';')[0] };
  }
  try {
    assert.equal((await call('/api/state')).status, 401);
    const sender = await call('/api/login', { account: '10001', password: '123456' });
    const recipient = await call('/api/login', { account: '10002', password: '123456' });
    assert.equal(sender.status, 200);
    assert.equal(recipient.status, 200);
    let senderState = (await call('/api/state', undefined, sender.cookie)).data;
    assert.equal(senderState.user.nickname, '张三');
    assert.equal(senderState.user.wechatId, 'wx_82e4197659c7e1af');
    assert.equal(senderState.user.wechatBoundQQ, '');
    assert.equal((await call('/api/action', { type: 'wechat.bind' }, sender.cookie)).status, 200);
    senderState = (await call('/api/state', undefined, sender.cookie)).data;
    assert.equal(senderState.user.wechatBoundQQ, '10001');
    assert.equal((await call('/api/action', { type: 'service.open', service: 'WeChat' }, recipient.cookie)).status, 200);
    assert.equal((await call('/api/action', { type: 'friend.add', service: 'WeChat', target: '10001', remark: '张三微信' }, recipient.cookie)).status, 200);
    assert.equal((await call('/api/action', { type: 'service.close', service: 'WeChat' }, sender.cookie)).status, 200);
    assert.equal((await call('/api/action', { type: 'wechat.unbind' }, sender.cookie)).status, 200);
    assert.equal((await call('/api/state', undefined, sender.cookie)).data.user.wechatBoundQQ, '');
    assert.equal((await call('/api/action', { type: 'service.open', service: 'WeChat' }, sender.cookie)).status, 200);
    assert.equal((await call('/api/action', { type: 'friend.add', service: 'WeChat', target: '10001', remark: '张三微信' }, recipient.cookie)).status, 400);
    assert.equal((await call('/api/action', { type: 'service.close', service: 'QQ' }, sender.cookie)).status, 200);
    assert.equal((await call('/api/action', { type: 'wechat.bind' }, sender.cookie)).status, 400);
    assert.equal((await call('/api/action', { type: 'service.open', service: 'QQ' }, sender.cookie)).status, 200);
    assert.equal((await call('/api/action', { type: 'wechat.rename', wechatId: 'zhangsanwx' }, sender.cookie)).status, 200);
    assert.equal((await call('/api/action', { type: 'wechat.rename', wechatId: 'another_wx' }, sender.cookie)).status, 400);
    assert.equal((await call('/api/register', { account: 'zhangsanwx', id: '新用户', password: '123456', confirm: '123456' })).status, 400);
    assert.match((await call('/api/state', undefined, recipient.cookie)).data.user.wechatId, /^wx_[0-9a-f]{16}$/);
    assert.equal((await call('/api/action', { type: 'wechat.rename', wechatId: 'zhangsanwx' }, recipient.cookie)).status, 400);
    assert.equal((await call('/api/action', { type: 'friend.add', service: 'WeChat', target: 'zhangsanwx', remark: '张三微信' }, recipient.cookie)).status, 200);
    const persistedUser = fs.readFileSync(path.join(temp, 'users.txt'), 'utf8').trim().split('\n')[0].split('|');
    assert.equal(persistedUser.length, 12);
    assert.equal(persistedUser[9], 'zhangsanwx');
    assert.equal(persistedUser[11], '');
    await call('/api/action', { type: 'message.send', kind: 'direct', target: '10002', text: '你好，李四' }, sender.cookie);
    await call('/api/action', { type: 'message.send', kind: 'group', target: '1001', text: '大家好' }, sender.cookie);
    let inbox = (await call('/api/state', undefined, recipient.cookie)).data;
    assert.equal(inbox.messages.length, 2);
    assert.equal(inbox.messages[0].sender, '10001');
    assert.equal(inbox.messages[0].text, '你好，李四');
    assert.ok(Number.isInteger(inbox.messages[0].at));
    assert.equal(inbox.messages[1].kind, 'group');
    const tcp = await call('/api/tcp', { ip: '127.0.0.1', port: tcpServer.address().port,
      target: '10002', text: 'TCP 测试' }, sender.cookie);
    assert.equal(tcp.status, 200, JSON.stringify(tcp.data));
    inbox = (await call('/api/state', undefined, recipient.cookie)).data;
    assert.equal(inbox.messages.filter(m => m.text === 'TCP 测试').length, 1);
    assert.equal(inbox.messages.at(-1).transport, 'tcp');
    await call('/api/action', { type: 'friend.recommend', from: 'QQ', to: 'WeChat', target: '10002' }, sender.cookie);
    assert.match(fs.readFileSync(path.join(temp, 'friends_10001.txt'), 'utf8'), /QQ,WeChat/);
    const groupDenied = await call('/api/action', { type: 'group', action: 'kick', groupId: '1001', target: '10001' }, recipient.cookie);
    assert.equal(groupDenied.status, 403);
    assert.equal(fs.readFileSync(path.join(temp, 'messages.txt'), 'utf8').trim().split('\n').length, 3);
  } finally {
    await Promise.all([new Promise(resolve => server.close(resolve)), new Promise(resolve => tcpServer.close(resolve))]);
    const resolved = path.resolve(temp), parent = path.resolve(os.tmpdir());
    if (resolved.startsWith(parent + path.sep) && path.basename(resolved).startsWith('microx-test-'))
      fs.rmSync(resolved, { recursive: true, force: true });
  }
});
