#include "ServiceBase_Hejr.h"
#include <algorithm>
using namespace std;

// 构造
ServiceBase_Hejr::ServiceBase_Hejr(string uid, string svcType)
{
    userId      = uid;
    serviceId   = uid;     // 默认与平台用户ID一致（QQ/微博共享）
    serviceType = svcType;
    loggedIn    = false;
}

// 虚析构
ServiceBase_Hejr::~ServiceBase_Hejr()
{
}

// ---------- 通用接口 ----------

// 平台已登录时，其它已开通服务免密自动登录
void ServiceBase_Hejr::autoLogin()
{
    loggedIn = true;
}

bool ServiceBase_Hejr::isLoggedIn() const
{
    return loggedIn;
}

void ServiceBase_Hejr::logout()
{
    loggedIn = false;
}

vector<string> ServiceBase_Hejr::getFriendList() const
{
    return friendIds;
}

string ServiceBase_Hejr::getUserId() const
{
    return userId;
}

string ServiceBase_Hejr::getServiceId() const
{
    return serviceId;
}

void ServiceBase_Hejr::setServiceId(string id)
{
    serviceId = id;
}

// 判断好友是否已存在
bool ServiceBase_Hejr::hasFriend(string friendId) const
{
    return find(friendIds.begin(), friendIds.end(), friendId)
           != friendIds.end();
}

// 静默加载好友（绕过输出，仅用于从文件恢复数据）
void ServiceBase_Hejr::loadFriend(string friendId)
{
    if (!hasFriend(friendId))
        friendIds.push_back(friendId);
}

// 查询与另一服务的共同好友（默认实现：对好友ID列表求交集）
vector<string> ServiceBase_Hejr::queryCommonFriends(ServiceBase_Hejr* other)
{
    vector<string> a = friendIds;
    vector<string> b = other->getFriendList();

    sort(a.begin(), a.end());
    sort(b.begin(), b.end());

    vector<string> result;
    set_intersection(a.begin(), a.end(),
                     b.begin(), b.end(),
                     back_inserter(result));
    return result;
}

