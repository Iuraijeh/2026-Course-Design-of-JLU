const root = document.querySelector('#root');
const modal = document.querySelector('#modal');
const toastElement = document.querySelector('#toast');
let snapshot = null;
let snapshotText = '';
let section = 'chats';
let selected = null;
let authMode = 'login';
let mobileDetail = false;
let modalSubmit = null;
let toastTimer;
let futureIdentityType = 'shared';

const icons = { chats: '◈', friends: '♙', groups: '♧', services: '▦', cross: '↗', tcp: '⇄', account: '⚙' };
const labels = { chats: '消息', friends: '好友', groups: '群组', services: '服务', cross: '跨服务', tcp: 'TCP', account: '账号' };
const e = value => String(value ?? '').replace(/[&<>"']/g, x => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[x]);
const time = ms => new Date(ms).toLocaleString('zh-CN', { month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit' });
const initials = text => String(text || '?').slice(0, 1).toUpperCase();
const me = () => snapshot?.user.account;
const nicknameOf = account => {
  const user = snapshot?.users.find(u => u.account === account);
  return user?.nickname || user?.id || '未知用户';
};
const myNickname = () => snapshot?.user.nickname || snapshot?.user.id || '';
const monthInShanghai = () => {
  const parts = new Intl.DateTimeFormat('en-US', {
    timeZone: 'Asia/Shanghai', year: 'numeric', month: '2-digit'
  }).formatToParts(new Date());
  return `${parts.find(part => part.type === 'year').value}-${parts.find(part => part.type === 'month').value}`;
};
const serviceName = service => service === 'WeChat' ? '微信' : service === 'Weibo' ? '微博' : service;
const toast = (message, error = false) => {
  toastElement.textContent = message;
  toastElement.className = `show${error ? ' error' : ''}`;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toastElement.className = '', 3300);
};

async function api(url, data) {
  const options = data === undefined ? {} : { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(data) };
  const response = await fetch(url, options);
  const result = await response.json();
  if (!response.ok) throw new Error(result.error || '操作失败');
  return result;
}
async function refresh(force = false) {
  try {
    const next = await api('/api/state');
    const text = JSON.stringify(next);
    if (force || text !== snapshotText) {
      snapshot = next;
      snapshotText = text;
      const active = document.activeElement;
      if (!force && active && ['INPUT', 'TEXTAREA', 'SELECT'].includes(active.tagName) &&
          active.closest('.workspace')) renderLive();
      else render();
    }
  } catch (error) {
    if (error.message === '请先登录') { snapshot = null; snapshotText = ''; render(); }
    else toast(error.message, true);
  }
}
async function action(data, success = '操作已完成') {
  try { await api('/api/action', data); await refresh(true); toast(success); }
  catch (error) { toast(error.message, true); }
}

function renderAuth() {
  const register = authMode === 'register';
  root.innerHTML = `<div class="auth-shell"><section class="auth-art">
    <div class="brand"><span class="logo" style="display:inline-grid;margin:0 12px 0 0;vertical-align:middle">微</span> 微X</div>
    <div><div class="eyebrow" style="color:#a8b9e3">CONNECTED, SIMPLY</div><h1>让每一次<br>交流更靠近。</h1>
    <p>消息、好友、群组与多服务管理，在一个清晰的空间里自然连接。</p></div>
    <div class="auth-orbit"></div></section>
    <section class="auth-card-wrap"><div class="auth-card"><div class="eyebrow">MICROX WORKSPACE</div>
    <h2>${register ? '创建账号' : '欢迎回来'}</h2><p class="muted">${register ? '填写信息，开始使用微X。' : '登录后查看你的消息与联系人。'}</p>
    <form id="auth-form"><div class="field"><label>平台登录账号号</label><input name="account" autocomplete="username" placeholder="请输入账号号" required></div>
    ${register ? `<div class="field"><label>昵称</label><input name="id" placeholder="用于聊天时显示" required></div>` : ''}
    <div class="field"><label>密码</label><input name="password" type="password" autocomplete="${register ? 'new-password' : 'current-password'}" minlength="6" maxlength="12" required></div>
    ${register ? `<div class="field"><label>确认密码</label><input name="confirm" type="password" autocomplete="new-password" required></div>` : ''}
    <button class="primary" type="submit">${register ? '注册账号' : '登录微X'}</button></form>
    <div class="auth-foot">${register ? '已经有账号？' : '还没有账号？'} <button class="auth-switch" data-switch-auth>${register ? '返回登录' : '立即注册'}</button></div>
    <div class="hint-box">本机初版 · 数据与现有控制台程序共用。可用已有的 10001、10002、10003 账号登录。</div>
    </div></section></div>`;
}

function chatEntries() {
  const direct = snapshot.friends.map(friend => ({ kind: 'direct', id: friend.account,
    name: nicknameOf(friend.account), subtitle: friend.remark && friend.remark !== nicknameOf(friend.account)
      ? `备注 ${friend.remark}` : '好友单聊' }));
  const groups = snapshot.groups.filter(g => g.members.includes(me())).map(g => ({ kind: 'group', id: g.id,
    name: g.name, subtitle: `${g.members.length} 位成员 · ${g.strategy === 'QQ' ? 'QQ 群模式' : '微信群模式'}` }));
  const entries = [...direct, ...groups];
  for (const entry of entries) {
    const messages = snapshot.messages.filter(m => entry.kind === 'direct'
      ? m.kind === 'direct' && (m.sender === entry.id && m.target === me() || m.sender === me() && m.target === entry.id)
      : m.kind === 'group' && m.target === entry.id);
    entry.last = messages.at(-1);
  }
  return entries.sort((a, b) => (b.last?.at || 0) - (a.last?.at || 0));
}
function listItem(entry, type = 'select') {
  const active = selected?.kind === entry.kind && selected?.id === entry.id;
  const last = entry.last;
  return `<button class="list-item ${active ? 'active' : ''}" data-${type}-kind="${e(entry.kind)}" data-${type}-id="${e(entry.id)}">
    <span class="avatar ${entry.kind === 'group' ? 'group' : ''}">${entry.kind === 'group' ? '群' : e(initials(entry.name))}</span>
    <span class="item-main"><span class="item-line"><span class="item-name">${e(entry.name)}</span>${last ? `<span class="item-time">${time(last.at)}</span>` : ''}</span>
    <span class="item-sub">${e(last?.text || entry.subtitle || '')}</span></span></button>`;
}
function pane() {
  let entries = [];
  if (section === 'chats') entries = chatEntries();
  else if (section === 'friends') entries = snapshot.friends.map(f => ({ kind: 'direct', id: f.account,
    name: f.remark || nicknameOf(f.account), subtitle: f.services.map(serviceName).join(' · ') }));
  else if (section === 'groups') entries = snapshot.groups.map(g => ({ kind: 'group', id: g.id,
    name: g.name, subtitle: `${g.members.length} 位成员 · ${g.strategy}` }));
  const add = section === 'friends' ? `<button class="icon-btn" title="添加好友" data-command="friend.add">+</button>` : '';
  return `<aside class="list-pane"><div class="pane-head"><div class="eyebrow">YOUR WORKSPACE</div>
    <div class="pane-title"><h2>${labels[section]}</h2>${add}</div>
    ${entries.length ? `<div class="search"><input id="filter-list" placeholder="搜索${labels[section]}" aria-label="搜索"></div>` : ''}</div>
    <div class="list-scroll">${entries.length ? entries.map(item => listItem(item)).join('') : sidebarInfo()}</div></aside>`;
}
function sidebarInfo() {
  if (section === 'chats') return `<div class="section-caption">还没有会话</div><p class="muted" style="padding:0 12px;font-size:13px">先添加好友或加入群组。</p>`;
  if (section === 'services') return dbServices().map(s => `<button class="list-item" data-service-view="${s}"><span class="avatar">${e(initials(serviceName(s)))}</span><span class="item-main"><span class="item-name">${e(serviceName(s))}</span><span class="item-sub">${snapshot.user.services.includes(s) ? '已开通' : '未开通'}</span></span></button>`).join('');
  if (section === 'cross') return `<div class="section-caption">好友关系</div><p class="muted" style="padding:0 12px;font-size:13px">查询共同好友，并将好友推荐到其他服务。</p>`;
  if (section === 'tcp') return `<div class="section-caption">网络连接</div><p class="muted" style="padding:0 12px;font-size:13px">按目标 IP 与端口发送 TCP 消息，关联账号历史。</p>`;
  if (section === 'account') return `<div class="section-caption">个人资料</div><p class="muted" style="padding:0 12px;font-size:13px">查看账号号，修改昵称和密码。</p>`;
  return `<div class="section-caption">暂无内容</div>`;
}
function dbServices() { return ['QQ', 'WeChat', 'Weibo']; }
function shell() {
  return `<div class="shell ${mobileDetail ? 'detail-open' : ''}"><nav class="rail" aria-label="主导航"><div class="logo">微</div>
    ${Object.keys(icons).map(key => `<button title="${labels[key]}" aria-label="${labels[key]}" class="${section === key ? 'active' : ''}" data-nav="${key}">${icons[key]}</button>`).join('')}
    <div class="spacer"></div><button class="tiny-avatar" title="${e(myNickname())}" data-nav="account">${e(initials(myNickname()))}</button></nav>
    ${pane()}<main class="workspace">${workspace()}</main></div>`;
}
function topbar(title, subtitle, actions = '') {
  return `<header class="topbar"><div class="toolbar"><button class="ghost mobile-back" style="display:none" data-back>←</button><div><h2>${e(title)}</h2><small>${e(subtitle)}</small></div></div>
    <div class="topbar-actions">${actions}</div></header>`;
}
function empty(title, text) {
  return `${topbar(labels[section], '微X · 本机工作空间')}<div class="empty"><div><div class="empty-icon">◌</div><h2>${e(title)}</h2><p>${e(text)}</p></div></div>`;
}
function historyFor(kind, id) {
  return snapshot.messages.filter(m => kind === 'direct'
    ? m.kind === 'direct' && (m.sender === me() && m.target === id || m.sender === id && m.target === me())
    : m.kind === 'group' && m.target === id);
}
function messageMarkup(message) {
  const own = message.sender === me();
  return `<div class="message-row ${own ? 'own' : ''}"><span class="avatar" style="width:34px;height:34px;flex-basis:34px;font-size:13px">${e(initials(nicknameOf(message.sender)))}</span>
    <div class="message-stack"><div class="message-meta">${e(nicknameOf(message.sender))} · ${time(message.at)}${message.transport === 'tcp' ? ' · TCP' : ''}</div>
    <div class="message-bubble">${e(message.text)}</div></div></div>`;
}
function chatView() {
  const entry = chatEntries().find(x => x.kind === selected?.kind && x.id === selected?.id);
  if (!entry) return empty('选择一个会话', '从左侧选择好友或群组，开始查看和发送消息。');
  const messages = historyFor(entry.kind, entry.id);
  return `${topbar(entry.name, entry.subtitle, `<span class="tag"><span class="status-dot"></span>${entry.kind === 'group' ? '群聊' : '单聊'}</span>`)}
    <div class="message-list" id="message-list">${messages.length ? messages.map(messageMarkup).join('') : `<div class="empty"><div><div class="empty-icon">✦</div><h2>开始对话</h2><p>这里会显示双方或群成员的历史消息、发送人和发送时间。</p></div></div>`}</div>
    <form class="composer" id="message-form"><textarea name="text" placeholder="输入消息…" maxlength="4000" required></textarea>
    <div class="composer-bar"><small>消息会自动保存到本机历史记录</small><button class="primary">发送消息 ↗</button></div></form>`;
}
function friendView() {
  const friend = snapshot.friends.find(f => f.account === selected?.id);
  if (!friend) return empty('管理你的好友', '选择一位好友查看资料，或点击左侧加号添加好友。');
  return `${topbar('好友资料', `平台账号号 ${friend.account}`)}<div class="main-scroll">
    <div class="detail-hero"><div class="avatar">${e(initials(friend.remark))}</div><div><h2>${e(friend.remark)}</h2><p>昵称 ${e(nicknameOf(friend.account))} · 平台账号号 ${e(friend.account)}</p></div></div>
    <div class="button-row"><button class="primary" data-command="friend.chat">发消息</button><button class="secondary" data-command="friend.edit">修改好友</button><button class="danger" data-command="friend.remove">删除好友</button></div>
    <div class="info-card"><div class="overline">关联服务</div><div class="chips">${friend.services.map(s => `<span class="chip">${e(serviceName(s))}</span>`).join('')}</div></div>
    <p class="footer-note">好友数据保存在现有 friends_${e(me())}.txt 文件中。</p></div>`;
}
function groupView() {
  const group = snapshot.groups.find(g => g.id === selected?.id);
  if (!group) return empty('探索你的群组', '选择群组查看成员、管理模式和群聊。');
  const member = group.members.includes(me());
  const owner = group.owner === me();
  const features = group.strategy === 'QQ'
    ? [['申请加入', '支持'], ['邀请加入', '支持'], ['管理员制度', '支持'], ['临时讨论组', '支持']]
    : [['申请加入', '不支持'], ['邀请加入', '支持'], ['管理员制度', '仅群主'], ['临时讨论组', '不支持']];
  return `${topbar(group.name, `群号 ${group.id} · ${group.strategy === 'QQ' ? 'QQ 群模式' : '微信群模式'}`)}
  <div class="main-scroll"><div class="detail-hero"><div class="avatar group">群</div><div><h2>${e(group.name)}</h2><p>${group.members.length} 位成员 · 群主 ${e(nicknameOf(group.owner))}</p></div></div>
  <div class="button-row">${member ? `<button class="primary" data-command="group.chat">打开群聊</button><button class="secondary" data-command="group.invite">邀请好友</button>
    <button class="secondary" data-command="group.subgroup">创建讨论组</button><button class="secondary" data-command="group.strategy">切换模式</button>
    ${owner ? `<button class="secondary" data-command="group.admin">设置管理员</button>` : `<button class="danger" data-command="group.quit">退出群组</button>`}
    <button class="danger" data-command="group.kick">移除成员</button>` : `<button class="primary" data-command="group.join">申请加入</button>`}</div>
  <div class="card-grid"><section class="info-card"><div class="overline">群成员</div><div class="members">${group.members.map(account => `<span class="member">${e(nicknameOf(account))}${account === group.owner ? ' · 群主' : group.admins.includes(account) && group.strategy === 'QQ' ? ' · 管理员' : ''}</span>`).join('')}</div></section>
  <section class="info-card"><div class="overline">管理特色</div><div class="feature-grid">${features.map(([label, value]) => `<div class="feature"><span class="muted">${label}</span><b>${value}</b></div>`).join('')}</div></section></div>
  <div class="info-card" style="margin-top:16px"><div class="overline">临时讨论组</div>${group.subgroups.length ? `<div class="list-simple">${group.subgroups.map(s => `<button type="button">${e(s.name)} · ${s.members.map(nicknameOf).map(e).join('、')}</button>`).join('')}</div>` : '<p class="muted">暂无临时讨论组</p>'}</div></div>`;
}
function servicesView() {
  const user = snapshot.user;
  const wechatOpen = user.services.includes('WeChat');
  const qqOpen = user.services.includes('QQ');
  const changedThisMonth = user.wechatIdChangedMonth === monthInShanghai();
  const futureDescription = futureIdentityType === 'shared'
    ? '未来产品可沿用不可修改的 QQ 账号号，与 QQ、微博共享服务身份。当前只预留类型选项。'
    : '未来产品可使用与 QQ 分开的独立账号号。具体修改和绑定规则由该产品决定；当前只预留类型选项。';
  const serviceDescription = service => {
    if (service === 'WeChat') return user.wechatId ? `独立微信账号号 ${user.wechatId}` : '开通后自动生成独立微信账号号';
    return `与${service === 'QQ' ? '微博' : 'QQ'}共用账号号 ${user.account}`;
  };
  return `${topbar('开通服务管理', '查看服务账号号与开通状态')}<div class="main-scroll">
  <div class="banner">平台仅使用平台账号号登录；QQ 与微博共用这个账号号，微信使用独立账号号。微信账号号不可用于平台登录，聊天中显示昵称。服务数据与控制台程序同步。</div>
  <div class="info-card"><div class="overline">服务入口</div>${dbServices().map(service => `<div class="toggle-row"><div><b>${e(serviceName(service))}</b>
    <div class="muted service-description">${e(serviceDescription(service))}</div></div>
    <button class="${user.services.includes(service) ? 'danger' : 'primary'}" data-service="${e(service)}">${user.services.includes(service) ? '关闭服务' : '开通服务'}</button></div>`).join('')}</div>
  <div class="info-card"><div class="overline">微信账号号</div><div class="identity-heading"><strong>${e(user.wechatId || '尚未生成')}</strong>
    <span class="tag">${wechatOpen ? '微信服务已开通' : '微信服务未开通'}</span></div>
    <p class="muted identity-copy">微信账号号由系统自动生成，全平台唯一，长度为 6–20 位，可使用字母、数字和下划线。自动生成不计入修改次数；之后每个自然月最多修改一次。</p>
    <div class="button-row"><button class="secondary" data-command="wechat.rename" ${wechatOpen && !changedThisMonth ? '' : 'disabled'}>修改微信账号号</button></div>
    <p class="identity-note">${changedThisMonth ? '本月已修改过微信账号号，下个自然月可再次修改。' : wechatOpen ? '本月还可以修改一次微信账号号。' : '开通微信服务后可修改账号号。'}</p>
    <div class="toggle-row"><div><b>绑定 QQ 账号号</b><div class="muted service-description">${user.wechatBoundQQ ? `已绑定 ${e(user.wechatBoundQQ)}` : '当前未绑定。绑定后可随时解绑，再重新绑定。'}</div></div>
      ${user.wechatBoundQQ ? '<button class="danger" data-command="wechat.unbind">解绑</button>' : `<button class="secondary" data-command="wechat.bind" ${wechatOpen && qqOpen ? '' : 'disabled'}>绑定当前 QQ 账号号</button>`}</div>
    ${!user.wechatBoundQQ && !(wechatOpen && qqOpen) ? '<p class="identity-note">绑定前需先开通并登录微信、QQ 服务。</p>' : ''}
  </div>
  <div class="info-card"><div class="overline">未来微X产品</div><h3>选择服务账号号类型</h3>
    <p class="muted identity-copy">这两种类型供未来产品使用，目前不会创建新产品。</p>
    <div class="identity-options" role="group" aria-label="未来产品账号号类型">
      <button type="button" class="identity-option ${futureIdentityType === 'shared' ? 'selected' : ''}" data-future-identity="shared" aria-pressed="${futureIdentityType === 'shared'}"><b>共享 QQ 账号号</b><span>类似 QQ、微博</span></button>
      <button type="button" class="identity-option ${futureIdentityType === 'independent' ? 'selected' : ''}" data-future-identity="independent" aria-pressed="${futureIdentityType === 'independent'}"><b>独立账号号</b><span>类似微信</span></button>
    </div><p class="identity-note">${e(futureDescription)}</p>
  </div></div>`;
}
function crossView() {
  const opened = snapshot.user.services;
  return `${topbar('跨服务好友', '查询共同好友与推荐好友')}<div class="main-scroll">
  <div class="card-grid"><section class="info-card"><h3>查询两服务共同好友</h3><p class="muted">选择两个已开通服务，查看同时关联的好友。</p>
  <form id="common-form"><div class="field"><label>来源服务</label><select name="from">${opened.map(s => `<option value="${e(s)}">${e(serviceName(s))}</option>`).join('')}</select></div><div class="field"><label>目标服务</label><select name="to">${opened.map(s => `<option value="${e(s)}">${e(serviceName(s))}</option>`).join('')}</select></div><button class="primary">查询共同好友</button></form><div id="common-result"></div></section>
  <section class="info-card"><h3>推荐好友到另一服务</h3><p class="muted">保留已有好友关系，并增加新的服务关联。</p>
  <form id="recommend-form"><div class="field"><label>好友平台账号号</label><select name="target">${snapshot.friends.map(f => `<option value="${e(f.account)}">${e(f.remark)} · ${e(f.account)}</option>`).join('')}</select></div>
  <div class="field"><label>来源服务</label><select name="from">${opened.map(s => `<option value="${e(s)}">${e(serviceName(s))}</option>`).join('')}</select></div><div class="field"><label>目标服务</label><select name="to">${opened.map(s => `<option value="${e(s)}">${e(serviceName(s))}</option>`).join('')}</select></div>
  <button class="primary" ${snapshot.friends.length ? '' : 'disabled'}>推荐好友</button></form></section></div></div>`;
}
function tcpView() {
  const tcpMessages = snapshot.messages.filter(m => m.transport === 'tcp').slice(-12).reverse();
  return `${topbar('TCP 点对点聊天', `接收端口 ${snapshot.tcp.port} · ${snapshot.tcp.listening ? '网页后端正在监听' : '由控制台或其他程序监听'}`)}<div class="main-scroll">
  <div class="banner warn">使用目标 IP 和端口发送消息时，也要填写收件账号。这样双方才能在账号聊天记录中看到发件人、内容和时间。</div>
  <div class="info-card"><h3>发送 TCP 消息</h3><form id="tcp-form"><div class="stat-list"><div class="field"><label>目标 IP</label><input name="ip" value="127.0.0.1" required></div>
  <div class="field"><label>目标端口</label><input name="port" type="number" min="1" max="65535" value="${snapshot.tcp.port}" required></div>
  <div class="field"><label>收件人平台账号号</label><input name="target" placeholder="例如 10002" required></div></div>
  <div class="field"><label>消息内容</label><textarea name="text" maxlength="4000" placeholder="输入要发送的消息" required></textarea></div><button class="primary">发送 TCP 消息 ↗</button></form></div>
  <div class="info-card tcp-history"><h3>最近 TCP 消息</h3>${tcpMessages.length ? tcpMessages.map(m => `<div class="toggle-row"><div><b>${e(nicknameOf(m.sender))} → ${e(nicknameOf(m.target))}</b><div class="muted" style="font-size:12px;margin-top:5px">${e(m.text)}</div></div><small class="muted">${time(m.at)}</small></div>`).join('') : '<p class="muted">暂无 TCP 消息</p>'}</div></div>`;
}
function accountView() {
  const user = snapshot.user;
  return `${topbar('账号设置', '个人资料与登录安全', `<button class="ghost" data-command="logout">退出登录</button>`)}<div class="main-scroll">
  <div class="detail-hero"><div class="avatar">${e(initials(myNickname()))}</div><div><h2>${e(myNickname())}</h2><p>平台登录账号号 ${e(user.account)}</p></div></div>
  <div class="stat-list"><div class="stat"><div class="stat-label">平台登录账号号</div><div class="stat-value">${e(user.account)}</div></div><div class="stat"><div class="stat-label">已开通服务</div><div class="stat-value">${user.services.length}</div></div><div class="stat"><div class="stat-label">好友</div><div class="stat-value">${snapshot.friends.length}</div></div><div class="stat"><div class="stat-label">已加入群组</div><div class="stat-value">${snapshot.groups.filter(g => g.members.includes(me())).length}</div></div></div>
  <div class="info-card" style="margin-top:22px"><h3>个人资料</h3><div class="toggle-row"><div><b>昵称</b><div class="muted" style="font-size:12px">${e(myNickname())} · 聊天时显示</div></div><button class="secondary" data-command="profile.id">修改</button></div>
  <div class="toggle-row"><div><b>账号密码</b><div class="muted" style="font-size:12px">定期更换密码，保护聊天记录</div></div><button class="secondary" data-command="profile.password">修改</button></div>
  <div class="toggle-row"><div><b>注册时间</b></div><span class="muted">${e(user.registered)}</span></div></div></div>`;
}
function workspace() {
  if (section === 'chats') return chatView();
  if (section === 'friends') return friendView();
  if (section === 'groups') return groupView();
  if (section === 'services') return servicesView();
  if (section === 'cross') return crossView();
  if (section === 'tcp') return tcpView();
  return accountView();
}
function render() {
  if (!snapshot) return renderAuth();
  root.innerHTML = shell();
  const list = document.querySelector('#message-list');
  if (list) list.scrollTop = list.scrollHeight;
}
function renderLive() {
  if (section !== 'chats' || !selected) return;
  const list = document.querySelector('#message-list');
  if (!list) return;
  const nearBottom = list.scrollHeight - list.scrollTop - list.clientHeight < 100;
  const messages = historyFor(selected.kind, selected.id);
  list.innerHTML = messages.length ? messages.map(messageMarkup).join('') : '';
  if (nearBottom) list.scrollTop = list.scrollHeight;
}

function showModal(title, fields, submit, description = '') {
  modalSubmit = submit;
  modal.innerHTML = `<form class="modal-inner" id="modal-form"><h2>${e(title)}</h2>${description ? `<p>${e(description)}</p>` : ''}
    ${fields.map(field => `<div class="field"><label>${e(field.label)}</label>${field.type === 'select'
      ? `<select name="${e(field.name)}">${field.options.map(opt => `<option value="${e(opt.value)}" ${opt.value === field.value ? 'selected' : ''}>${e(opt.label)}</option>`).join('')}</select>`
      : `<input name="${e(field.name)}" type="${e(field.type || 'text')}" value="${e(field.value || '')}" ${field.required === false ? '' : 'required'} ${field.minlength ? `minlength="${field.minlength}"` : ''} ${field.maxlength ? `maxlength="${field.maxlength}"` : ''} ${field.pattern ? `pattern="${e(field.pattern)}"` : ''} ${field.placeholder ? `placeholder="${e(field.placeholder)}"` : ''} ${field.identityTarget ? 'data-identity-target' : ''}></div>`}
    ${field.type === 'select' ? '</div>' : ''}`).join('')}
    <div class="modal-actions"><button type="button" class="ghost" data-close-modal>取消</button><button class="primary">确认</button></div></form>`;
  modal.showModal();
  modal.querySelector('input,select')?.focus();
}
const serviceOptions = (extra = []) => [...new Set([...snapshot.user.services, ...extra])]
  .map(x => ({ value: x, label: serviceName(x) }));
function command(key) {
  const friend = snapshot.friends.find(f => f.account === selected?.id);
  const group = snapshot.groups.find(g => g.id === selected?.id);
  switch (key) {
    case 'friend.add': return showModal('添加好友', [
      { name: 'service', label: '来源服务', type: 'select', options: serviceOptions() },
      { name: 'target', label: '好友 QQ 账号号', placeholder: '填写对方的 QQ 账号号', identityTarget: true },
      { name: 'remark', label: '备注名' }
    ], data => action({ type: 'friend.add', ...data }, '好友已添加'));
    case 'friend.chat': section = 'chats'; selected = { kind: 'direct', id: friend.account }; return render();
    case 'friend.edit': return showModal('修改好友', [
      { name: 'remark', label: '备注名', value: friend.remark },
      { name: 'service', label: '来源服务', type: 'select', options: serviceOptions(friend.services), value: friend.services[0] }
    ], data => action({ type: 'friend.update', target: friend.account, ...data }, '好友已更新'));
    case 'friend.remove':
      if (confirm(`删除好友 ${friend.remark}？`)) action({ type: 'friend.remove', target: friend.account }, '好友已删除'); return;
    case 'group.chat': section = 'chats'; selected = { kind: 'group', id: group.id }; return render();
    case 'group.join': return action({ type: 'group', action: 'join', groupId: group.id }, '已加入群组');
    case 'group.quit':
      if (confirm(`退出群组 ${group.name}？`)) action({ type: 'group', action: 'quit', groupId: group.id }, '已退出群组'); return;
    case 'group.invite': return showModal('邀请好友入群', [{ name: 'target', label: '好友平台账号号' }],
      data => action({ type: 'group', action: 'invite', groupId: group.id, ...data }, '已邀请入群'));
    case 'group.kick': return showModal('移除群成员', [{ name: 'target', label: '成员账号' }],
      data => action({ type: 'group', action: 'kick', groupId: group.id, ...data }, '成员已移除'));
    case 'group.admin': return showModal('设置管理员', [
      { name: 'target', label: '成员账号' },
      { name: 'action', label: '操作', type: 'select', options: [{ value: 'admin.add', label: '设为管理员' }, { value: 'admin.remove', label: '取消管理员' }] }
    ], data => action({ type: 'group', groupId: group.id, ...data }, '管理员设置已更新'));
    case 'group.subgroup': return showModal('创建临时讨论组', [{ name: 'name', label: '讨论组名称' }],
      data => action({ type: 'group', action: 'subgroup', groupId: group.id, ...data }, '讨论组已创建'));
    case 'group.strategy': return showModal('切换群管理模式', [{ name: 'strategy', label: '新模式', type: 'select',
      options: [{ value: 'QQ', label: 'QQ 群模式' }, { value: 'WeChat', label: '微信群模式' }], value: group.strategy }],
      data => action({ type: 'group', action: 'strategy', groupId: group.id, ...data }, '群模式已切换'));
    case 'profile.id': return showModal('修改昵称', [{ name: 'id', label: '新昵称', value: myNickname() }],
      data => action({ type: 'profile.id', ...data }, '昵称已更新'));
    case 'wechat.rename': return showModal('修改微信账号号', [{
      name: 'wechatId', label: '新的微信账号号', value: snapshot.user.wechatId,
      minlength: 6, maxlength: 20, pattern: '[A-Za-z0-9_]{6,20}'
    }], data => action({ type: 'wechat.rename', ...data }, '微信账号号已更新'),
    '每个自然月最多修改一次；系统自动生成的账号号不计入次数。');
    case 'wechat.bind': return action({ type: 'wechat.bind' }, '已绑定当前 QQ 账号号');
    case 'wechat.unbind': return action({ type: 'wechat.unbind' }, '已解绑 QQ 账号号');
    case 'profile.password': return showModal('修改密码', [
      { name: 'oldPassword', label: '原密码', type: 'password' },
      { name: 'password', label: '新密码（6–12 位）', type: 'password', minlength: 6 }
    ], data => action({ type: 'profile.password', ...data }, '密码已修改'));
    case 'logout': return api('/api/logout', {}).then(() => { snapshot = null; snapshotText = ''; render(); });
  }
}

document.addEventListener('click', event => {
  const button = event.target.closest('button');
  if (!button) return;
  if (button.matches('[data-switch-auth]')) { authMode = authMode === 'login' ? 'register' : 'login'; render(); }
  else if (button.dataset.nav) { section = button.dataset.nav; selected = null; mobileDetail = false; render(); }
  else if (button.dataset.selectKind) { selected = { kind: button.dataset.selectKind, id: button.dataset.selectId }; mobileDetail = true; render(); }
  else if (button.dataset.serviceView) { mobileDetail = true; render(); }
  else if (button.dataset.futureIdentity) {
    const scrollTop = document.querySelector('.main-scroll')?.scrollTop || 0;
    futureIdentityType = button.dataset.futureIdentity;
    render();
    document.querySelector('.main-scroll').scrollTop = scrollTop;
  }
  else if (button.dataset.command) command(button.dataset.command);
  else if (button.dataset.service) action({ type: snapshot.user.services.includes(button.dataset.service) ? 'service.close' : 'service.open', service: button.dataset.service }, '服务状态已更新');
  else if (button.matches('[data-close-modal]')) modal.close();
  else if (button.matches('[data-back]')) { mobileDetail = false; render(); }
});
document.addEventListener('change', event => {
  if (!event.target.matches('#modal-form select[name="service"]')) return;
  const target = event.target.closest('form').querySelector('input[data-identity-target]');
  if (!target) return;
  const service = event.target.value;
  target.closest('.field').querySelector('label').textContent = `好友${serviceName(service)}账号号`;
  target.placeholder = `填写对方的${serviceName(service)}账号号`;
});
document.addEventListener('input', event => {
  if (event.target.id !== 'filter-list') return;
  const term = event.target.value.toLowerCase();
  document.querySelectorAll('.list-scroll .list-item').forEach(item => item.style.display = item.textContent.toLowerCase().includes(term) ? '' : 'none');
});
document.addEventListener('submit', async event => {
  event.preventDefault();
  const form = event.target;
  const data = Object.fromEntries(new FormData(form));
  try {
    if (form.id === 'auth-form') {
      await api(authMode === 'register' ? '/api/register' : '/api/login', data);
      if (authMode === 'register') { authMode = 'login'; render(); toast('注册成功，请登录'); }
      else { await refresh(true); toast('登录成功'); }
    } else if (form.id === 'modal-form') {
      modal.close(); await modalSubmit(data);
    } else if (form.id === 'message-form') {
      await api('/api/action', { type: 'message.send', kind: selected.kind, target: selected.id, text: data.text });
      form.reset(); await refresh(true);
    } else if (form.id === 'recommend-form') {
      await action({ type: 'friend.recommend', ...data }, '好友已推荐到目标服务');
    } else if (form.id === 'common-form') {
      const common = snapshot.friends.filter(f => f.services.includes(data.from) && f.services.includes(data.to));
      document.querySelector('#common-result').innerHTML = `<div class="section-caption">查询结果 · ${common.length} 位好友</div>${common.length ? common.map(f => `<span class="chip">${e(f.remark)} · ${e(f.account)}</span>`).join(' ') : '<p class="muted">没有共同好友</p>'}`;
    } else if (form.id === 'tcp-form') {
      await api('/api/tcp', data); form.querySelector('[name=text]').value = '';
      await refresh(true); toast('TCP 消息已发送并保存');
    }
  } catch (error) { toast(error.message, true); }
});

render();
refresh();
setInterval(() => { if (snapshot) refresh(); }, 1500);
