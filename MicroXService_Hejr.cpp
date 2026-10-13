#include "MicroXService_Hejr.h"
#include <iostream>
#include <algorithm>
using namespace std;

MicroXService_Hejr::MicroXService_Hejr(string uid, string svcType)
    : ServiceBase_Hejr(uid, svcType)
{
    // QQ 和微博默认共用平台账号；微信账号号由平台创建服务后设置。
    serviceId = uid;
}

bool MicroXService_Hejr::login(string password)
{
    // 密码已由 Platform_Hejr::login 统一校验；服务层只建立会话状态。
    (void)password;
    loggedIn = true;
    return true;
}

void MicroXService_Hejr::addFriend(string friendId)
{
    if (!hasFriend(friendId))
    {
        friendIds.push_back(friendId);
        cout << "[统一服务] 已添加好友: " << friendId << endl;
    }
}

void MicroXService_Hejr::removeFriend(string friendId)
{
    vector<string>::iterator it = find(friendIds.begin(), friendIds.end(), friendId);
    if (it != friendIds.end())
    {
        friendIds.erase(it);
        cout << "[统一服务] 已删除好友: " << friendId << endl;
    }
}

string MicroXService_Hejr::getServiceType()
{
    return serviceType;
}
