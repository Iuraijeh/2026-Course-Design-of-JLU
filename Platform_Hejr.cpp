#include "Platform_Hejr.h"
#include "MessageStore_Hejr.h"
#include <iostream>
#include <limits>
#include <cctype>
#include <set>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
using namespace std;

static string normalizeAccountNumber(string account)
{
    for (size_t i = 0; i < account.size(); ++i)
        account[i] = (char)tolower((unsigned char)account[i]);
    return account;
}

static bool isValidAccountNumber(const string& account)
{
    if (account.empty() || account.size() > 64) return false;
    for (size_t i = 0; i < account.size(); ++i)
    {
        unsigned char c = (unsigned char)account[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9'))) return false;
    }
    return true;
}

static bool isValidNickname(const string& nickname)
{
    if (nickname.empty() || nickname.find_first_of("|\r\n") != string::npos)
        return false;
    size_t characters = 0;
    bool hasNonWhitespace = false;
    for (size_t i = 0; i < nickname.size(); ++i)
    {
        if (((unsigned char)nickname[i] & 0xc0) != 0x80) ++characters;
        if (!isspace((unsigned char)nickname[i])) hasNonWhitespace = true;
    }
    return hasNonWhitespace && characters <= 80;
}

static bool isValidWeChatId(const string& value)
{
    if (value.size() < 6 || value.size() > 20) return false;
    for (size_t i = 0; i < value.size(); ++i)
    {
        unsigned char c = (unsigned char)value[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

static string currentLocalMonth()
{
    time_t now = time(NULL);
    tm localTime;
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    ostringstream month;
    month << (1900 + localTime.tm_year) << "-"
          << setw(2) << setfill('0') << (localTime.tm_mon + 1);
    return month.str();
}

// ============================================================
// 辅助函数：安全读取整数
// 防止用户输入字母导致 cin 进入错误状态死循环
// ============================================================
int Platform_Hejr::readInt() const
{
    int n;
    while (!(cin >> n))
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "输入无效，请输入数字: ";
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return n;
}

// ============================================================
// 构造：加载所有数据，首次运行时初始化测试数据
// ============================================================
Platform_Hejr::Platform_Hejr()
{
    userContainer    = new UserContainer_Hejr();
    // 群、好友和服务数据在认证成功后再加载。
    groupContainer   = NULL;
    currentUser      = NULL;
    serviceContainer = NULL;
    friendContainer  = NULL;
    tcpChat          = new TcpChat_Hejr();
    running          = true;
    messageWatcher   = NULL;
    watchMessages    = false;

    // 若无用户数据，初始化测试数据
    if (userContainer->getAllUsers().empty())
    {
        cout << "[平台] 首次运行，正在初始化测试数据..." << endl;
        initData();
        userContainer->save();
    }
    // TCP 接收器随平台启动并持续运行；退出平台时才停止。
    if (!tcpChat->startServer(9527))
        cout << "[TCP] 常开接收器启动失败，请检查 9527 端口是否被占用。" << endl;
}

// ============================================================
// 析构：登出当前用户，释放容器内存（析构时自动保存数据）
// ============================================================
Platform_Hejr::~Platform_Hejr()
{
    logout();

    delete tcpChat;

    delete userContainer;
    delete groupContainer;
}

// ============================================================
// 首次运行初始化测试数据：3 个用户 + 好友关系
// ============================================================
void Platform_Hejr::initData()
{
    // --- 创建用户 ---
    // 10001 张三：开通 QQ + WeChat
    User_Hejr u1("10001", "张三", "2000-05-15", "2010-09-01", "北京");
    u1.addService("QQ");
    u1.addService("WeChat");
    u1.addFriendId("10002");
    u1.addFriendId("10003");
    u1.addGroupId("1001");

    // 10002 李四：开通 QQ
    User_Hejr u2("10002", "李四", "1999-03-20", "2011-01-15", "上海");
    u2.addService("QQ");
    u2.addFriendId("10001");
    u2.addGroupId("1001");

    // 10003 王五：开通 WeChat
    User_Hejr u3("10003", "王五", "2001-11-08", "2012-06-10", "广州");
    u3.addService("WeChat");
    u3.addFriendId("10001");
    u3.addGroupId("1002");

    userContainer->addUser(u1);
    userContainer->addUser(u2);
    userContainer->addUser(u3);

    // --- 创建好友文件 ---
    // 10001 的好友
    {
        FriendContainer_Hejr fc("10001");
        fc.addFriend(Friend_Hejr("10002", "李四", "QQ"));
        fc.addFriend(Friend_Hejr("10003", "王五", "WeChat"));
        fc.save();
    }
    // 10002 的好友
    {
        FriendContainer_Hejr fc("10002");
        fc.addFriend(Friend_Hejr("10001", "张三", "QQ"));
        fc.save();
    }
    // 10003 的好友
    {
        FriendContainer_Hejr fc("10003");
        fc.addFriend(Friend_Hejr("10001", "张三", "WeChat"));
        fc.save();
    }

    cout << "[平台] 测试数据初始化完成（3 个用户、好友关系）。" << endl;
}

// 让用户档案中的群号和群对象成员表保持一致，兼容旧数据文件。
void Platform_Hejr::syncUserGroupsFromStorage()
{
    vector<User_Hejr>& users = userContainer->getAllUsers();
    for (size_t i = 0; i < users.size(); ++i)
    {
        vector<string> gids = users[i].getGroupIds();
        for (size_t j = 0; j < gids.size(); ++j)
        {
            Group_Hejr* group = groupContainer->findByGroupId(gids[j]);
            if (group != NULL) group->loadMember(users[i].getAccountNumber());
        }
    }

    // 群主必然属于该群；同时将迁移后的预设群写回其用户档案。
    vector<Group_Hejr*> groups = groupContainer->getAllGroups();
    for (size_t i = 0; i < groups.size(); ++i)
    {
        User_Hejr* owner = userContainer->findUserById(groups[i]->getOwnerId());
        if (owner != NULL) owner->addGroupId(groups[i]->getGroupId());
    }
}

static bool storedGroupMember(const string& account, const string& groupId)
{
    ifstream file("groups.txt");
    string line;
    while (getline(file, line))
    {
        istringstream input(line);
        string field;
        vector<string> fields;
        while (getline(input, field, '|')) fields.push_back(field);
        if (fields.size() < 5 || fields[0] != groupId) continue;
        istringstream members(fields[4]);
        while (getline(members, field, ','))
            if (field == account) return true;
    }
    return false;
}

void Platform_Hejr::startMessageWatcher()
{
    stopMessageWatcher();
    if (currentUser == NULL) return;
    const string account = currentUser->getAccountNumber();
    watchMessages = true;
    messageWatcher = new thread([this, account]() {
        set<string> seen;
        vector<MessageRecord_Hejr> existing = MessageStore_Hejr::readAll();
        for (size_t i = 0; i < existing.size(); ++i) seen.insert(existing[i].id);
        while (watchMessages)
        {
            this_thread::sleep_for(chrono::milliseconds(850));
            if (!watchMessages) break;
            vector<MessageRecord_Hejr> all = MessageStore_Hejr::readAll();
            for (size_t i = 0; i < all.size(); ++i)
            {
                const MessageRecord_Hejr& message = all[i];
                if (!seen.insert(message.id).second || message.sender == account) continue;
                if ((message.kind == "direct" && message.target == account) ||
                    (message.kind == "group" && storedGroupMember(account, message.target)))
                    cout << "\n[新消息 " << MessageStore_Hejr::formatTime(message.at)
                         << "] " << MessageStore_Hejr::displayIdForAccount(message.sender)
                         << " -> " << (message.kind == "group" ? "群 " + message.target
                              : MessageStore_Hejr::displayIdForAccount(message.target))
                         << ": " << message.text << "\n请选择: " << flush;
            }
        }
    });
}

void Platform_Hejr::stopMessageWatcher()
{
    watchMessages = false;
    if (messageWatcher != NULL)
    {
        messageWatcher->join();
        delete messageWatcher;
        messageWatcher = NULL;
    }
}

void Platform_Hejr::refreshFromDisk()
{
    if (currentUser == NULL) return;
    const string account = currentUser->getAccountNumber();
    userContainer->load("users.txt");
    currentUser = userContainer->findUserById(account);
    if (currentUser == NULL) { logout(); return; }
    if (groupContainer != NULL) groupContainer->load("groups.txt");
    if (friendContainer != NULL) friendContainer->load();
    delete serviceContainer;
    serviceContainer = new ServiceContainer_Hejr(account);
    vector<string> opened = currentUser->getOpenedServices();
    for (size_t i = 0; i < opened.size(); ++i)
    {
        serviceContainer->openService(opened[i]);
        ServiceBase_Hejr* service = serviceContainer->getService(opened[i]);
        if (service != NULL)
        {
            if (opened[i] == "WeChat") service->setServiceId(currentUser->getWeChatId());
            service->autoLogin();
        }
    }
    if (friendContainer != NULL)
    {
        vector<Friend_Hejr>& friends = friendContainer->getAllFriends();
        for (size_t i = 0; i < friends.size(); ++i)
        {
            vector<string> types = friends[i].getServiceTypes();
            for (size_t j = 0; j < types.size(); ++j)
            {
                ServiceBase_Hejr* service = serviceContainer->getService(types[j]);
                if (service != NULL) service->loadFriend(friends[i].getFriendId());
            }
        }
    }
}

// ============================================================
// 检查是否已登录
// ============================================================
bool Platform_Hejr::checkLogin() const
{
    if (currentUser == NULL)
    {
        cout << "请先登录！" << endl;
        return false;
    }
    return true;
}

// ============================================================
// 登录：一次认证，所有已开通服务自动登录
// ============================================================
bool Platform_Hejr::login(string userId, string password)
{
    userId = normalizeAccountNumber(userId);
    userContainer->load("users.txt");
    // 查找用户
    User_Hejr* user = userContainer->findUserById(userId);
    if (user == NULL)
    {
        cout << "[平台] 用户 " << userId << " 不存在。" << endl;
        return false;
    }

    // 认证必须先于任何服务、好友和群数据加载。
    if (!user->checkPassword(password))
    {
        cout << "[平台] 密码错误，登录失败。" << endl;
        return false;
    }

    vector<string> opened = user->getOpenedServices();
    if (opened.empty())
    {
        cout << "[平台] 用户 " << userId << " 尚未开通任何服务。" << endl;
        return false;
    }

    // 若已有登录会话，先登出
    if (currentUser != NULL)
        logout();

    // 设置当前用户
    currentUser = user;

    // 认证成功后才加载群数据及其成员关系。
    if (groupContainer == NULL)
        groupContainer = new GroupContainer_Hejr();
    syncUserGroupsFromStorage();

    // 创建服务容器，并实例化所有已开通的服务
    serviceContainer = new ServiceContainer_Hejr(userId);
    for (size_t i = 0; i < opened.size(); i++)
    {
        serviceContainer->openService(opened[i]);
        ServiceBase_Hejr* service = serviceContainer->getService(opened[i]);
        if (service != NULL && opened[i] == "WeChat")
            service->setServiceId(user->getWeChatId());
    }

    // 创建好友容器
    friendContainer = new FriendContainer_Hejr(userId);

    // 将好友容器中的好友按服务类型同步到各服务对象的好友列表。
    vector<Friend_Hejr>& allFriends = friendContainer->getAllFriends();
    for (size_t i = 0; i < allFriends.size(); i++)
    {
        vector<string> types = allFriends[i].getServiceTypes();
        for (size_t j = 0; j < types.size(); ++j)
        {
            ServiceBase_Hejr* s = serviceContainer->getService(types[j]);
            if (s != NULL) s->loadFriend(allFriends[i].getFriendId());
        }
    }
    cout << "[平台] 好友数据已同步到对应服务。" << endl;

    // 使用任一已开通入口完成一次认证；不再要求选择 QQ、微信或微博。
    ServiceBase_Hejr* svc = serviceContainer->getService(opened[0]);
    if (svc == NULL || !svc->login(password))
    {
        cout << "[平台] 登录失败。" << endl;
        logout();
        return false;
    }

    cout << "[平台] 统一认证成功，正在联动其它已开通服务..." << endl;

    // 自动登录其它已开通服务
    vector<ServiceBase_Hejr*> allSvcs = serviceContainer->getAllServices();
    for (size_t i = 0; i < allSvcs.size(); i++)
    {
        if (allSvcs[i] != svc && !allSvcs[i]->isLoggedIn())
        {
            allSvcs[i]->autoLogin();
            cout << "[平台] " << allSvcs[i]->getServiceType()
                 << " 已简单确认并自动登录。" << endl;
        }
    }

    cout << "[平台] 用户 " << user->getNickname()
         << " (帐号 " << user->getAccountNumber() << ") 登录完成。" << endl;
    serviceContainer->listAll();
    startMessageWatcher();
    return true;
}

bool Platform_Hejr::registerUser(string accountNumber, string password,
                                 string passwordConfirmation, string userId)
{
    accountNumber = normalizeAccountNumber(accountNumber);
    if (!isValidAccountNumber(accountNumber))
    {
        cout << "[注册] 账号号必须为 1–64 位字母或数字。" << endl;
        return false;
    }
    if (userContainer->isAccountNumberUsed(accountNumber))
    {
        cout << "[注册] 账号号已存在，请重新填写。" << endl;
        return false;
    }
    if (userContainer->isWeChatIdUsed(accountNumber))
    {
        cout << "[注册] 该账号号已是其他用户的微信账号号，请重新填写。" << endl;
        return false;
    }
    if (password.size() < 6 || password.size() > 12)
    {
        cout << "[注册] 密码长度必须为 6-12 位。" << endl;
        return false;
    }
    if (password != passwordConfirmation)
    {
        cout << "[注册] 两次输入的密码不一致。" << endl;
        return false;
    }
    if (!isValidNickname(userId))
    {
        cout << "[注册] 昵称须为 1–80 个字符，且不能含有分隔符或换行。" << endl;
        return false;
    }

    User_Hejr user(accountNumber, userId, "1970-01-01", "2026-01-01", "");
    user.changePassword(password);
    // 统一微X账号注册后默认开通三个入口。
    user.addService("QQ");
    user.addService("WeChat");
    user.addService("Weibo");
    userContainer->addUser(user);
    userContainer->save();
    cout << "[注册] 注册成功，请使用账号号登录。" << endl;
    return true;
}

bool Platform_Hejr::changeUserId(string newUserId)
{
    if (!checkLogin()) return false;
    if (!isValidNickname(newUserId))
    {
        cout << "[账号设置] 昵称须为 1–80 个字符，且不能含有分隔符或换行。" << endl;
        return false;
    }
    currentUser->setId(newUserId);
    userContainer->save();
    cout << "[账号设置] 昵称已修改为 " << newUserId
         << "，账号号保持不变。" << endl;
    return true;
}

bool Platform_Hejr::changeWeChatId(string newWeChatId)
{
    if (!checkLogin()) return false;
    if (!currentUser->isServiceOpened("WeChat"))
    {
        cout << "[微信] 请先开通微信服务。" << endl;
        return false;
    }
    if (!isValidWeChatId(newWeChatId))
    {
        cout << "[微信] 微信账号号必须是 6–20 位字母、数字或下划线。" << endl;
        return false;
    }
    if (newWeChatId == currentUser->getWeChatId())
    {
        cout << "[微信] 微信账号号未变化。" << endl;
        return true;
    }
    string month = currentLocalMonth();
    if (currentUser->getWeChatIdChangedMonth() == month)
    {
        cout << "[微信] 本自然月已修改过微信账号号，请下个月再试。" << endl;
        return false;
    }
    if (userContainer->isWeChatIdUsed(newWeChatId, currentUser->getAccountNumber()))
    {
        cout << "[微信] 该账号号已被使用，请换一个。" << endl;
        return false;
    }
    currentUser->setWeChatId(newWeChatId);
    currentUser->setWeChatIdChangedMonth(month);
    ServiceBase_Hejr* service = serviceContainer->getService("WeChat");
    if (service != NULL) service->setServiceId(newWeChatId);
    userContainer->save();
    cout << "[微信] 微信账号号已修改为 " << newWeChatId << "。" << endl;
    return true;
}

bool Platform_Hejr::bindWeChatToCurrentQQ()
{
    if (!checkLogin()) return false;
    ServiceBase_Hejr* wechat = serviceContainer->getService("WeChat");
    ServiceBase_Hejr* qq = serviceContainer->getService("QQ");
    if (wechat == NULL || qq == NULL || !qq->isLoggedIn())
    {
        cout << "[微信] 绑定前请开通微信并登录当前账号的 QQ 服务。" << endl;
        return false;
    }
    currentUser->setWeChatBoundQQ(currentUser->getAccountNumber());
    userContainer->save();
    cout << "[微信] 已绑定当前登录的 QQ 账号号 "
         << currentUser->getWeChatBoundQQ() << "。" << endl;
    return true;
}

bool Platform_Hejr::unbindWeChatQQ()
{
    if (!checkLogin()) return false;
    if (currentUser->getWeChatBoundQQ().empty())
    {
        cout << "[微信] 当前未绑定 QQ。" << endl;
        return false;
    }
    currentUser->setWeChatBoundQQ("");
    userContainer->save();
    cout << "[微信] 已解除 QQ 绑定。" << endl;
    return true;
}

bool Platform_Hejr::changePassword(string oldPassword, string newPassword)
{
    if (!checkLogin()) return false;
    if (!currentUser->checkPassword(oldPassword))
    {
        cout << "[平台] 原密码错误，密码未修改。" << endl;
        return false;
    }
    if (newPassword.size() < 6 || newPassword.size() > 12)
    {
        cout << "[平台] 新密码长度必须为 6-12 位。" << endl;
        return false;
    }
    currentUser->changePassword(newPassword);
    userContainer->save();
    cout << "[平台] 密码修改成功，已保存到 users.txt。" << endl;
    return true;
}

// ============================================================
// 登出：释放当前用户的服务/好友容器
// ============================================================
void Platform_Hejr::logout()
{
    stopMessageWatcher();
    if (serviceContainer != NULL)
    {
        delete serviceContainer;
        serviceContainer = NULL;
    }
    if (friendContainer != NULL)
    {
        delete friendContainer;
        friendContainer = NULL;
    }
    if (groupContainer != NULL)
    {
        delete groupContainer;
        groupContainer = NULL;
    }
    currentUser = NULL;
}

// ============================================================
// 开通服务：同时更新 User_Hejr 和 ServiceContainer
// ============================================================
bool Platform_Hejr::openService(string serviceType)
{
    if (!checkLogin()) return false;
    if (!serviceContainer->openService(serviceType)) return false;
    currentUser->addService(serviceType);
    if (serviceType == "WeChat") userContainer->ensureWeChatId(*currentUser);
    ServiceBase_Hejr* service = serviceContainer->getService(serviceType);
    if (service != NULL)
    {
        if (serviceType == "WeChat") service->setServiceId(currentUser->getWeChatId());
        service->autoLogin();
    }
    userContainer->save();
    return true;
}

// ============================================================
// 关闭服务
// ============================================================
bool Platform_Hejr::closeService(string serviceType)
{
    if (!checkLogin()) return false;

    if (!serviceContainer->closeService(serviceType)) return false;
    currentUser->removeService(serviceType);
    userContainer->save();
    return true;
}

string Platform_Hejr::resolveFriendAccount(const string& input,
                                           const string& serviceType) const
{
    if (input.empty()) return "";
    User_Hejr* user = NULL;
    if (serviceType == "WeChat")
    {
        user = userContainer->findUserByWeChatId(input);
        if (user == NULL)
        {
            const vector<User_Hejr>& all = userContainer->getAllUsers();
            for (size_t i = 0; i < all.size(); ++i)
            {
                if (all[i].isServiceOpened("WeChat") &&
                    !all[i].getWeChatBoundQQ().empty() &&
                    normalizeAccountNumber(all[i].getWeChatBoundQQ()) ==
                        normalizeAccountNumber(input))
                {
                    user = userContainer->findUserById(all[i].getAccountNumber());
                    break;
                }
            }
        }
    }
    else
    {
        user = userContainer->findUserById(input);
        if (user == NULL && serviceType.empty())
            user = userContainer->findUserByWeChatId(input);
    }
    if (user == NULL || (!serviceType.empty() &&
        !user->isServiceOpened(serviceType))) return "";
    return user->getAccountNumber();
}

// ============================================================
// 跨服务好友：从 fromService 的好友列表中取人加入 toService
// ============================================================
bool Platform_Hejr::addFriendFromService(string fromServiceType,
                                        string friendId,
                                        string toServiceType)
{
    if (!checkLogin()) return false;
    string resolved = resolveFriendAccount(friendId, fromServiceType);
    if (!resolved.empty()) friendId = resolved;

    ServiceBase_Hejr* fromSvc = serviceContainer->getService(fromServiceType);
    ServiceBase_Hejr* toSvc   = serviceContainer->getService(toServiceType);

    if (fromSvc == NULL)
    {
        cout << "[平台] 未开通 " << fromServiceType << " 服务。" << endl;
        return false;
    }
    if (toSvc == NULL)
    {
        cout << "[平台] 未开通 " << toServiceType << " 服务。" << endl;
        return false;
    }

    // 检查 fromService 是否有该好友
    if (!fromSvc->hasFriend(friendId))
    {
        cout << "[平台] " << fromServiceType << " 服务中无好友 "
             << friendId << "，无法推荐。" << endl;
        return false;
    }

    // 添加到 toService
    Friend_Hejr* friendInfo = friendContainer->findFriend(friendId);
    if (friendInfo == NULL)
    {
        cout << "[平台] 好友持久化记录不存在，无法推荐。" << endl;
        return false;
    }
    if (!friendInfo->hasServiceType(toServiceType))
        friendInfo->addServiceType(toServiceType);
    toSvc->addFriend(friendId);
    friendContainer->save();
    cout << "[平台] 已从 " << fromServiceType << " 推荐好友 "
         << friendId << " 到 " << toServiceType << "。" << endl;
    return true;
}

// ============================================================
// 展示群管理特色
// ============================================================
void Platform_Hejr::showGroupFeature(string groupId)
{
    Group_Hejr* g = groupContainer->findByGroupId(groupId);
    if (g == NULL)
    {
        cout << "[平台] 群 " << groupId << " 不存在。" << endl;
        return;
    }
    g->showFeature();
}

// ============================================================
// 动态切换群管理模式（成员数据不变）
// ============================================================
bool Platform_Hejr::transformGroup(string groupId, string newStrategyType)
{
    Group_Hejr* g = groupContainer->findByGroupId(groupId);
    if (g == NULL)
    {
        cout << "[平台] 群 " << groupId << " 不存在。" << endl;
        return false;
    }

    GroupStrategy_Hejr* strat = groupContainer->getStrategy(newStrategyType);
    if (strat == NULL)
    {
        cout << "[平台] 未知策略类型: " << newStrategyType << endl;
        return false;
    }

    g->setStrategy(strat);
    groupContainer->save("groups.txt");
    return true;
}

// ============================================================
// 主循环
// ============================================================
void Platform_Hejr::run()
{
    while (running)
    {
    // 启动门禁：未通过认证前不显示账号服务，也不加载业务数据。
    while (running && currentUser == NULL)
    {
        userContainer->load("users.txt");
        string accountNumber, password, passwordConfirmation, userId;
        cout << "\n========================================" << endl;
        cout << "          微X平台登录 / 注册" << endl;
        cout << "========================================" << endl;
        cout << "  1. 登录" << endl;
        cout << "  2. 注册" << endl;
        cout << "  0. 退出" << endl;
        cout << "请选择: ";
        int action = readInt();
        if (action == 0)
        {
            running = false;
            break;
        }
        if (action == 1)
        {
            cout << "请输入账号号: ";
            getline(cin, accountNumber);
            cout << "请输入密码: ";
            getline(cin, password);
            if (!login(accountNumber, password))
                cout << "认证未通过，请重新登录。" << endl;
        }
        else if (action == 2)
        {
            cout << "请输入账号号（仅字母和数字）: ";
            getline(cin, accountNumber);
            cout << "请输入密码（6-12位）: ";
            getline(cin, password);
            cout << "请再次输入密码: ";
            getline(cin, passwordConfirmation);
            cout << "请输入昵称: ";
            getline(cin, userId);
            registerUser(accountNumber, password, passwordConfirmation, userId);
        }
        else
            cout << "无效选项，请重新选择。" << endl;
    }

    while (running && currentUser != NULL)
    {
        refreshFromDisk();
        if (currentUser == NULL) break;
        cout << "\n";
        cout << "========================================" << endl;
        cout << "     模拟即时通信系统平台（微X系列）" << endl;
        cout << "========================================" << endl;
        if (currentUser != NULL)
        {
            cout << " 当前昵称: " << currentUser->getNickname()
                 << "（账号号: " << currentUser->getAccountNumber() << ")" << endl;
        }
        else
        {
            cout << " 当前状态: 未登录" << endl;
        }
        cout << "----------------------------------------" << endl;
        cout << "  1. 消息中心（单聊 / 群聊 / 历史记录）" << endl;
        cout << "  2. 好友管理" << endl;
        cout << "  3. 群管理" << endl;
        cout << "  4. 账号设置" << endl;
        cout << "  5. 开通服务管理" << endl;
        cout << "  6. 跨服务好友" << endl;
        cout << "  7. 群特色展示与变换" << endl;
        cout << "  8. TCP 点对点聊天" << endl;
        cout << "  9. 退出登录" << endl;
        cout << "  0. 退出系统" << endl;
        cout << "========================================" << endl;
        cout << "请选择: ";

        int choice = readInt();

        switch (choice)
        {
        case 1:
            chatMenu();
            break;
        case 2:
            friendMenu();
            break;
        case 3:
            groupMenu();
            break;
        case 4:
            accountMenu();
            break;
        case 5:
            serviceMenu();
            break;
        case 6:
            crossServiceMenu();
            break;
        case 7:
            groupFeatureMenu();
            break;
        case 8:
            tcpChatMenu();
            break;
        case 9:
            cout << "正在退出登录。" << endl;
            logout();
            break;
        case 0:
            cout << "正在保存数据并退出..." << endl;
            running = false;
            break;
        default:
            cout << "无效选项，请重新选择。" << endl;
            break;
        }
    }
    }
}

// ============================================================
// 好友管理子菜单
// ============================================================
void Platform_Hejr::friendMenu()
{
    if (!checkLogin()) return;

    bool back = false;
    while (!back)
    {
        refreshFromDisk();
        cout << "\n--- 好友管理 ---" << endl;
        cout << "  1. 添加好友" << endl;
        cout << "  2. 删除好友" << endl;
        cout << "  3. 修改好友信息" << endl;
        cout << "  4. 查询好友列表" << endl;
        cout << "  5. 查询两服务共同好友" << endl;
        cout << "  0. 返回主菜单" << endl;
        cout << "请选择: ";

        int choice = readInt();

        switch (choice)
        {
        case 1:
        {
            string fid, remark, svcType;
            cout << "请输入好友账号号（微信可输入微信账号号）: ";
            getline(cin, fid);
            cout << "请输入备注名: ";
            getline(cin, remark);
            cout << "来源服务 (QQ/WeChat/Weibo): ";
            getline(cin, svcType);

            ServiceBase_Hejr* service = serviceContainer->getService(svcType);
            if (service == NULL)
            {
                cout << "请先开通该服务。" << endl;
                break;
            }
            string account = resolveFriendAccount(fid, svcType);
            if (account.empty())
            {
                cout << "未找到该服务账号号。" << endl;
                break;
            }
            if (account == currentUser->getAccountNumber())
            {
                cout << "不能添加自己为好友。" << endl;
                break;
            }
            fid = account;

            Friend_Hejr f(fid, remark, svcType);
            if (friendContainer->addFriend(f))
            {
                currentUser->addFriendId(fid);
                service->addFriend(fid);
                friendContainer->save();
                userContainer->save();
            }
            break;
        }
        case 2:
        {
            string fid;
            cout << "请输入要删除的好友账号号（也可输入微信账号号）: ";
            getline(cin, fid);
            string resolved = resolveFriendAccount(fid, "");
            if (!resolved.empty()) fid = resolved;
            Friend_Hejr* friendInfo = friendContainer->findFriend(fid);
            if (friendInfo != NULL)
            {
                vector<string> types = friendInfo->getServiceTypes();
                if (friendContainer->removeFriend(fid))
                {
                    for (size_t i = 0; i < types.size(); ++i)
                    {
                        ServiceBase_Hejr* service = serviceContainer->getService(types[i]);
                        if (service != NULL) service->removeFriend(fid);
                    }
                    currentUser->removeFriendId(fid);
                    friendContainer->save();
                    userContainer->save();
                }
            }
            else cout << "好友不存在。" << endl;
            break;
        }
        case 3:
        {
            string fid, remark, svcType;
            cout << "请输入好友账号号（也可输入微信账号号）: ";
            getline(cin, fid);
            cout << "请输入新备注名: ";
            getline(cin, remark);
            cout << "请输入新来源服务: ";
            getline(cin, svcType);
            string resolved = resolveFriendAccount(fid, "");
            if (!resolved.empty()) fid = resolved;
            Friend_Hejr* friendInfo = friendContainer->findFriend(fid);
            if (friendInfo != NULL)
            {
                vector<string> oldTypes = friendInfo->getServiceTypes();
                if (friendContainer->modifyFriend(fid, remark, svcType))
                {
                    for (size_t i = 0; i < oldTypes.size(); ++i)
                    {
                        ServiceBase_Hejr* service = serviceContainer->getService(oldTypes[i]);
                        if (service != NULL) service->removeFriend(fid);
                    }
                    ServiceBase_Hejr* service = serviceContainer->getService(svcType);
                    if (service != NULL) service->addFriend(fid);
                    friendContainer->save();
                }
            }
            else cout << "好友不存在。" << endl;
            break;
        }
        case 4:
            friendContainer->listAll();
            break;
        case 5:
        {
            string svc1, svc2;
            cout << "请输入第一个服务 (QQ/WeChat/Weibo): ";
            getline(cin, svc1);
            cout << "请输入第二个服务 (QQ/WeChat/Weibo): ";
            getline(cin, svc2);

            ServiceBase_Hejr* s1 = serviceContainer->getService(svc1);
            ServiceBase_Hejr* s2 = serviceContainer->getService(svc2);
            if (s1 == NULL || s2 == NULL)
            {
                cout << "服务未开通。" << endl;
                break;
            }
            vector<string> common = s1->queryCommonFriends(s2);
            cout << svc1 << " 与 " << svc2 << " 的共同好友（"
                 << common.size() << " 个）：" << endl;
            for (size_t i = 0; i < common.size(); i++)
                cout << "  " << common[i] << endl;
            break;
        }
        case 0:
            back = true;
            break;
        default:
            cout << "无效选项。" << endl;
            break;
        }
    }
}

// ============================================================
// 群管理子菜单
// ============================================================
void Platform_Hejr::groupMenu()
{
    if (!checkLogin()) return;

    bool back = false;
    while (!back)
    {
        refreshFromDisk();
        cout << "\n--- 群管理 ---" << endl;
        cout << "  1. 列出所有群" << endl;
        cout << "  2. 查看群成员" << endl;
        cout << "  3. 申请加入群" << endl;
        cout << "  4. 邀请好友入群" << endl;
        cout << "  5. 退群" << endl;
        cout << "  6. 挨T（踢人）" << endl;
        cout << "  7. 设置管理员" << endl;
        cout << "  8. 创建临时讨论组" << endl;
        cout << "  0. 返回主菜单" << endl;
        cout << "请选择: ";

        int choice = readInt();

        switch (choice)
        {
        case 1:
            groupContainer->listAll();
            break;
        case 2:
        {
            string gid;
            cout << "请输入群号: ";
            getline(cin, gid);
            Group_Hejr* g = groupContainer->findByGroupId(gid);
            if (g) g->listMembers();
            else cout << "群不存在。" << endl;
            break;
        }
        case 3:
        {
            string gid;
            cout << "请输入群号: ";
            getline(cin, gid);
            Group_Hejr* g = groupContainer->findByGroupId(gid);
            if (g && g->applyJoin(currentUser->getAccountNumber()))
            {
                currentUser->addGroupId(gid);
                groupContainer->save("groups.txt");
                userContainer->save();
            }
            else cout << "群不存在。" << endl;
            break;
        }
        case 4:
        {
            string gid, fid;
            cout << "请输入群号: ";
            getline(cin, gid);
            cout << "请输入被邀请人的平台账号号: ";
            getline(cin, fid);
            Group_Hejr* g = groupContainer->findByGroupId(gid);
            if (g && g->inviteJoin(fid, currentUser->getAccountNumber()))
            {
                User_Hejr* invited = userContainer->findUserById(fid);
                if (invited != NULL) invited->addGroupId(gid);
                groupContainer->save("groups.txt");
                userContainer->save();
            }
            else cout << "群不存在。" << endl;
            break;
        }
        case 5:
        {
            string gid;
            cout << "请输入群号: ";
            getline(cin, gid);
            Group_Hejr* g = groupContainer->findByGroupId(gid);
            if (g && g->quitGroup(currentUser->getAccountNumber()))
            {
                currentUser->removeGroupId(gid);
                groupContainer->save("groups.txt");
                userContainer->save();
            }
            else cout << "群不存在。" << endl;
            break;
        }
        case 6:
        {
            string gid, target;
            cout << "请输入群号: ";
            getline(cin, gid);
            cout << "请输入要踢出的用户账号号: ";
            getline(cin, target);
            Group_Hejr* g = groupContainer->findByGroupId(gid);
            if (g && g->kickMember(currentUser->getAccountNumber(), target))
            {
                User_Hejr* removed = userContainer->findUserById(target);
                if (removed != NULL) removed->removeGroupId(gid);
                groupContainer->save("groups.txt");
                userContainer->save();
            }
            else cout << "群不存在。" << endl;
            break;
        }
        case 7:
        {
            string gid, uid;
            cout << "请输入群号: ";
            getline(cin, gid);
            cout << "请输入要设为管理员的用户账号号: ";
            getline(cin, uid);
            Group_Hejr* g = groupContainer->findByGroupId(gid);
            if (g && g->addAdmin(currentUser->getAccountNumber(), uid))
                groupContainer->save("groups.txt");
            else cout << "群不存在。" << endl;
            break;
        }
        case 8:
        {
            string gid, name;
            cout << "请输入群号: ";
            getline(cin, gid);
            cout << "请输入讨论组名称: ";
            getline(cin, name);
            Group_Hejr* g = groupContainer->findByGroupId(gid);
            if (g && g->createSubgroup(name, currentUser->getAccountNumber()))
                groupContainer->save("groups.txt");
            else cout << "群不存在。" << endl;
            break;
        }
        case 0:
            back = true;
            break;
        default:
            cout << "无效选项。" << endl;
            break;
        }
    }
}

// ============================================================
// 账号设置子菜单
// ============================================================
void Platform_Hejr::accountMenu()
{
    if (!checkLogin()) return;

    bool back = false;
    while (!back)
    {
        refreshFromDisk();
        cout << "\n--- 账号设置 ---" << endl;
        cout << " 平台账号号: " << currentUser->getAccountNumber() << "（不可修改）" << endl;
        cout << " 当前昵称: " << currentUser->getNickname() << endl;
        if (currentUser->isServiceOpened("WeChat"))
        {
            cout << " 微信账号号: " << currentUser->getWeChatId() << endl;
            cout << " 已绑定QQ: " << (currentUser->getWeChatBoundQQ().empty()
                ? "未绑定" : currentUser->getWeChatBoundQQ()) << endl;
        }
        cout << "  1. 修改昵称" << endl;
        cout << "  2. 修改密码" << endl;
        cout << "  3. 修改微信账号号（每自然月最多一次）" << endl;
        cout << "  4. 绑定当前登录的 QQ 账号号" << endl;
        cout << "  5. 解除微信与 QQ 的绑定" << endl;
        cout << "  0. 返回主菜单" << endl;
        cout << "请选择: ";

        int choice = readInt();
        switch (choice)
        {
        case 1:
        {
            string newUserId;
            cout << "请输入新的昵称: ";
            getline(cin, newUserId);
            changeUserId(newUserId);
            break;
        }
        case 2:
        {
            string oldPassword, newPassword, confirmPassword;
            cout << "请输入原密码: ";
            getline(cin, oldPassword);
            cout << "请输入新密码（6-12位）: ";
            getline(cin, newPassword);
            cout << "请再次输入新密码: ";
            getline(cin, confirmPassword);
            if (newPassword != confirmPassword)
                cout << "两次输入的新密码不一致。" << endl;
            else if (newPassword.size() < 6 || newPassword.size() > 12)
                cout << "新密码长度必须为 6-12 位。" << endl;
            else
                changePassword(oldPassword, newPassword);
            break;
        }
        case 3:
        {
            string newWeChatId;
            cout << "请输入新的微信账号号（6–20 位字母、数字或下划线）: ";
            getline(cin, newWeChatId);
            changeWeChatId(newWeChatId);
            break;
        }
        case 4:
            bindWeChatToCurrentQQ();
            break;
        case 5:
            unbindWeChatQQ();
            break;
        case 0:
            back = true;
            break;
        default:
            cout << "无效选项。" << endl;
            break;
        }
    }
}

// ============================================================
// 开通服务管理子菜单
// ============================================================
void Platform_Hejr::serviceMenu()
{
    if (!checkLogin()) return;

    bool back = false;
    while (!back)
    {
        refreshFromDisk();
        cout << "\n--- 开通服务管理 ---" << endl;
        cout << "  1. 开通服务" << endl;
        cout << "  2. 关闭服务" << endl;
        cout << "  3. 查看已开通服务" << endl;
        cout << "  4. 了解未来产品：共用 QQ 账号号" << endl;
        cout << "  5. 了解未来产品：独立账号号" << endl;
        cout << "  0. 返回主菜单" << endl;
        cout << "请选择: ";

        int choice = readInt();

        switch (choice)
        {
        case 1:
        {
            cout << "可开通: 1.QQ  2.WeChat  3.Weibo" << endl;
            cout << "请选择: ";
            int svc = readInt();
            string svcType;
            if (svc == 1)      svcType = "QQ";
            else if (svc == 2) svcType = "WeChat";
            else if (svc == 3) svcType = "Weibo";
            else { cout << "无效选择。" << endl; break; }

            openService(svcType);
            break;
        }
        case 2:
        {
            cout << "可关闭: 1.QQ  2.WeChat  3.Weibo" << endl;
            cout << "请选择: ";
            int svc = readInt();
            string svcType;
            if (svc == 1)      svcType = "QQ";
            else if (svc == 2) svcType = "WeChat";
            else if (svc == 3) svcType = "Weibo";
            else { cout << "无效选择。" << endl; break; }

            closeService(svcType);
            break;
        }
        case 3:
            serviceContainer->listAll();
            if (currentUser->isServiceOpened("WeChat"))
                cout << "微信绑定 QQ: " << (currentUser->getWeChatBoundQQ().empty()
                    ? "未绑定" : currentUser->getWeChatBoundQQ()) << endl;
            break;
        case 4:
            cout << "未来的微X产品可与 QQ、微博一样共用平台账号号。当前仅展示类型，尚未创建产品。" << endl;
            break;
        case 5:
            cout << "未来的微X产品可与微信一样使用独立账号号，并由用户主动关联 QQ。当前仅展示类型，尚未创建产品。" << endl;
            break;
        case 0:
            back = true;
            break;
        default:
            cout << "无效选项。" << endl;
            break;
        }
    }
}

// ============================================================
// 跨服务好友子菜单
// ============================================================
void Platform_Hejr::crossServiceMenu()
{
    if (!checkLogin()) return;

    bool back = false;
    while (!back)
    {
        refreshFromDisk();
        cout << "\n--- 跨服务好友 ---" << endl;
        cout << "  1. 从某服务推荐好友到另一服务" << endl;
        cout << "  0. 返回主菜单" << endl;
        cout << "请选择: ";

        int choice = readInt();

        switch (choice)
        {
        case 1:
        {
            string fromSvc, toSvc, fid;
            cout << "来源服务 (QQ/WeChat/Weibo): ";
            getline(cin, fromSvc);
            cout << "目标服务 (QQ/WeChat/Weibo): ";
            getline(cin, toSvc);
            cout << "请输入好友账号号（微信可输入微信账号号）: ";
            getline(cin, fid);
            addFriendFromService(fromSvc, fid, toSvc);
            break;
        }
        case 0:
            back = true;
            break;
        default:
            cout << "无效选项。" << endl;
            break;
        }
    }
}

// ============================================================
// 群特色展示与动态变换子菜单
// ============================================================
void Platform_Hejr::groupFeatureMenu()
{
    bool back = false;
    while (!back)
    {
        refreshFromDisk();
        cout << "\n--- 群特色展示与变换 ---" << endl;
        cout << "  1. 展示群当前管理特色" << endl;
        cout << "  2. 动态切换群管理模式" << endl;
        cout << "  0. 返回主菜单" << endl;
        cout << "请选择: ";

        int choice = readInt();

        switch (choice)
        {
        case 1:
        {
            string gid;
            cout << "请输入群号: ";
            getline(cin, gid);
            showGroupFeature(gid);
            break;
        }
        case 2:
        {
            string gid, stratType;
            cout << "请输入群号: ";
            getline(cin, gid);
            cout << "选择新模式: 1.QQ群模式  2.微信群模式" << endl;
            cout << "请选择: ";
            int s = readInt();
            if (s == 1)      stratType = "QQ";
            else if (s == 2) stratType = "WeChat";
            else { cout << "无效选择。" << endl; break; }

            transformGroup(gid, stratType);
            break;
        }
        case 0:
            back = true;
            break;
        default:
            cout << "无效选项。" << endl;
            break;
        }
    }
}

// ============================================================
// 账号单聊与群聊：与网页共用 messages.txt。
// ============================================================
void Platform_Hejr::chatMenu()
{
    if (!checkLogin()) return;
    bool back = false;
    while (!back)
    {
        refreshFromDisk();
        if (currentUser == NULL) return;
        cout << "\n--- 消息中心 ---" << endl;
        cout << "  1. 查看好友聊天记录" << endl;
        cout << "  2. 给好友发送消息" << endl;
        cout << "  3. 查看群聊天记录" << endl;
        cout << "  4. 发送群消息" << endl;
        cout << "  0. 返回主菜单" << endl;
        cout << "请选择: ";
        int choice = readInt();
        if (choice == 0) { back = true; continue; }
        if (choice < 1 || choice > 4) { cout << "无效选项。" << endl; continue; }
        string target;
        cout << (choice <= 2 ? "好友账号: " : "群号: ");
        getline(cin, target);
        const string account = currentUser->getAccountNumber();
        if (choice <= 2)
        {
            if (userContainer->findUserById(target) == NULL)
            { cout << "账号不存在。" << endl; continue; }
            if (choice == 2 && friendContainer->findFriend(target) == NULL)
            { cout << "请先添加对方为好友。" << endl; continue; }
        }
        else
        {
            Group_Hejr* group = groupContainer->findByGroupId(target);
            if (group == NULL || !group->isMember(account))
            { cout << "只有群成员可以查看和发送群消息。" << endl; continue; }
        }
        const string kind = choice <= 2 ? "direct" : "group";
        if (choice == 1 || choice == 3)
        {
            vector<MessageRecord_Hejr> messages =
                MessageStore_Hejr::conversation(account, kind, target);
            cout << "共 " << messages.size() << " 条历史消息：" << endl;
            for (size_t i = 0; i < messages.size(); ++i)
                cout << "[" << MessageStore_Hejr::formatTime(messages[i].at) << "] "
                     << MessageStore_Hejr::displayIdForAccount(messages[i].sender)
                     << ": " << messages[i].text << endl;
        }
        else
        {
            string content;
            cout << "消息内容: ";
            getline(cin, content);
            if (content.empty()) { cout << "消息不能为空。" << endl; continue; }
            MessageRecord_Hejr message = MessageStore_Hejr::create(
                kind, account, target, content, "console");
            if (MessageStore_Hejr::append(message))
                cout << "已发送并保存 [" << MessageStore_Hejr::formatTime(message.at) << "]。" << endl;
            else cout << "消息保存失败。" << endl;
        }
    }
}

// ============================================================
// TCP 点对点聊天子菜单（选做）
// ============================================================
void Platform_Hejr::tcpChatMenu()
{
    bool back = false;
    while (!back)
    {
        refreshFromDisk();
        cout << "\n--- TCP 点对点聊天 ---" << endl;
        cout << "  TCP 接收状态: " << (tcpChat->isListening() ? "常开中" : "启动失败")
             << "（端口 9527）" << endl;
        cout << "  1. 查看本机 IP" << endl;
        cout << "  2. 发送消息" << endl;
        cout << "  0. 返回主菜单" << endl;
        cout << "请选择: ";

        int choice = readInt();
        switch (choice)
        {
        case 1:
            cout << "本机 IP: " << tcpChat->getLocalIP() << endl;
            break;
        case 2:
        {
            string ip, message, target;
            cout << "目标 IP: ";
            getline(cin, ip);
            cout << "目标端口: ";
            int port = readInt();
            cout << "收件账号: ";
            getline(cin, target);
            if (userContainer->findUserById(target) == NULL)
            { cout << "收件账号不存在。" << endl; break; }
            cout << "消息内容: ";
            getline(cin, message);
            tcpChat->sendMessage(ip, port, message,
                                 currentUser->getAccountNumber(), target);
            break;
        }
        case 0:
            back = true;
            break;
        default:
            cout << "无效选项。" << endl;
            break;
        }
    }
}

