#ifndef SERVICEBASE_HEJR_H
#define SERVICEBASE_HEJR_H

#include <string>
#include <vector>
#include "User_Hejr.h"
using namespace std;

// ============================================================
// 微X服务抽象基类 ServiceBase_Hejr
// 说明：
//   - 所有微X服务（QQ / 微信 / 微博 / ...）的统一接口
//   - 定义纯虚函数 login / addFriend / removeFriend /
//     queryCommonFriends / getServiceType，由派生类实现
//   - 提供通用能力：autoLogin（平台联动免密登录）、好友列表维护等
//   - 类名后缀 _Hejr 为姓名缩写占位符，请替换
// ============================================================
class ServiceBase_Hejr
{
protected:
    string userId;                // 关联的平台用户ID
    string serviceId;             // 本服务自身的ID（QQ/微信/微博可能不同）
    string serviceType;           // 服务类型标识（"QQ" / "WeChat" / "Weibo"）
    bool   loggedIn;              // 登录状态
    vector<string> friendIds;     // 该服务下的好友ID列表

public:
    // 构造：传入平台用户ID与服务类型
    ServiceBase_Hejr(string uid, string svcType);

    // 虚析构（基类必须有虚析构）
    virtual ~ServiceBase_Hejr();

    // ---------- 纯虚接口（派生类必须实现） ----------
    virtual bool   login(string password) = 0;
    virtual void   addFriend(string friendId) = 0;
    virtual void   removeFriend(string friendId) = 0;
    virtual string getServiceType() = 0;

    // ---------- 虚接口（基类提供默认实现，子类可复用或重写） ----------
    // 查询本服务与另一服务的共同好友
    virtual vector<string> queryCommonFriends(ServiceBase_Hejr* other);

    // ---------- 通用接口（基类提供默认实现，可复用） ----------
    // 平台已登录时免密自动登录
    void autoLogin();

    bool   isLoggedIn() const;
    void   logout();

    vector<string> getFriendList() const;
    string getUserId() const;
    string getServiceId() const;
    void   setServiceId(string id);

    // 判断某好友是否已在本服务好友列表中
    bool   hasFriend(string friendId) const;

    // 静默加载好友（绕过输出，仅用于从文件恢复数据）
    void   loadFriend(string friendId);
};

#endif // SERVICEBASE_HEJR_H


