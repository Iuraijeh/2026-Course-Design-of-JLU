#ifndef PLATFORM_HEJR_H
#define PLATFORM_HEJR_H

#include <string>
#include <thread>
#include <atomic>
#include "UserContainer_Hejr.h"
#include "GroupContainer_Hejr.h"
#include "ServiceContainer_Hejr.h"
#include "FriendContainer_Hejr.h"
#include "TcpChat_Hejr.h"
using namespace std;

// ============================================================
// 平台总控类 Platform_Hejr
// 职责：
//   1. 持有用户容器、群容器，管理当前登录用户的服务/好友容器
//   2. 登录联动：一个服务登录成功 → 其它已开通服务自动登录
//   3. 跨服务好友：从某服务好友列表中添加好友到另一服务
//   4. 群特色展示与动态切换管理策略
//   5. 菜单驱动（数字选择，支持返回上级）
//   6. 首次运行初始化测试数据
// 类名后缀 _Hejr 为姓名缩写占位符，请替换。
// ============================================================
class Platform_Hejr
{
private:
    UserContainer_Hejr*   userContainer;     // 全局用户容器
    GroupContainer_Hejr*  groupContainer;   // 全局群容器
    User_Hejr*             currentUser;       // 当前登录用户（未登录为 NULL）
    ServiceContainer_Hejr* serviceContainer; // 当前用户已开通的服务
    FriendContainer_Hejr*  friendContainer;  // 当前用户的好友容器
    TcpChat_Hejr*           tcpChat;         // 选做的点对点聊天模块
    bool running;                           // 主循环控制
    std::thread* messageWatcher;
    std::atomic<bool> watchMessages;

    // ---------- 内部辅助 ----------
    // 检查是否已登录，未登录则提示
    bool checkLogin() const;

    // 安全读取整数（防止输入字母导致 cin 死循环）
    int readInt() const;

    // 首次运行初始化测试数据
    void initData();

    // ---------- 子菜单 ----------
    void friendMenu();         // 好友管理
    void groupMenu();          // 群管理
    void serviceMenu();        // 开通管理
    void accountMenu();        // 账号设置
    void crossServiceMenu();   // 跨服务好友
    void groupFeatureMenu();   // 群特色展示与变换
    void tcpChatMenu();        // TCP 点对点聊天
    void chatMenu();           // 账号单聊、群聊与历史记录
    void syncUserGroupsFromStorage();
    void refreshFromDisk();
    void startMessageWatcher();
    void stopMessageWatcher();
    string resolveFriendAccount(const string& input, const string& serviceType) const;

public:
    // 构造：加载数据，首次运行时初始化
    Platform_Hejr();

    // 析构：保存数据并释放内存
    ~Platform_Hejr();

    // ---------- 登录 / 登出 ----------
    // 一次统一认证；任一已开通服务确认后，其他服务自动登录。
    bool login(string userId, string password);

    // 注册：平台账号号唯一且不可修改，第二列为昵称。
    bool registerUser(string accountNumber, string password,
                      string passwordConfirmation, string userId);

    // 修改当前昵称（平台账号号始终不变）。
    bool changeUserId(string newUserId);

    bool changeWeChatId(string newWeChatId);
    bool bindWeChatToCurrentQQ();
    bool unbindWeChatQQ();

    // 修改当前账号密码：校验旧密码后立即持久化。
    bool changePassword(string oldPassword, string newPassword);

    // 登出：释放当前用户的服务/好友容器
    void logout();

    // ---------- 开通 / 关闭服务 ----------
    bool openService(string serviceType);
    bool closeService(string serviceType);

    // ---------- 跨服务好友 ----------
    // 从 fromServiceType 的好友列表中，将 friendId 添加到 toServiceType
    bool addFriendFromService(string fromServiceType,
                               string friendId,
                               string toServiceType);

    // ---------- 群特色 ----------
    void showGroupFeature(string groupId);
    bool transformGroup(string groupId, string newStrategyType);

    // ---------- 主循环 ----------
    void run();
};

#endif // PLATFORM_HEJR_H


