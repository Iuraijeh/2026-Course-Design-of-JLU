#include "Friend_Hejr.h"
#include <iostream>
#include <algorithm>
#include <sstream>
using namespace std;

// 默认构造
Friend_Hejr::Friend_Hejr()
{
    friendId    = "";
    remark      = "";
}

// 带参构造
Friend_Hejr::Friend_Hejr(string id, string rmk, string svc)
{
    friendId    = id;
    remark      = rmk;
    stringstream stream(svc);
    string item;
    while (getline(stream, item, ','))
        if (!item.empty()) serviceTypes.push_back(item);
}

// 获取好友ID
string Friend_Hejr::getFriendId() const
{
    return friendId;
}

// 设置好友ID
void Friend_Hejr::setFriendId(string id)
{
    friendId = id;
}

// 获取备注名
string Friend_Hejr::getRemark() const
{
    return remark;
}

// 设置备注名
void Friend_Hejr::setRemark(string rmk)
{
    remark = rmk;
}

// 获取所属服务类型
string Friend_Hejr::getServiceType() const
{
    string result;
    for (size_t i = 0; i < serviceTypes.size(); ++i)
    {
        if (i > 0) result += ",";
        result += serviceTypes[i];
    }
    return result;
}

// 设置所属服务类型
void Friend_Hejr::setServiceType(string svc)
{
    serviceTypes.clear();
    stringstream stream(svc);
    string item;
    while (getline(stream, item, ','))
        if (!item.empty()) serviceTypes.push_back(item);
}

vector<string> Friend_Hejr::getServiceTypes() const { return serviceTypes; }

bool Friend_Hejr::hasServiceType(string svc) const
{
    return find(serviceTypes.begin(), serviceTypes.end(), svc) != serviceTypes.end();
}

bool Friend_Hejr::addServiceType(string svc)
{
    if (svc.empty() || hasServiceType(svc)) return false;
    serviceTypes.push_back(svc);
    return true;
}

bool Friend_Hejr::removeServiceType(string svc)
{
    vector<string>::iterator it = find(serviceTypes.begin(), serviceTypes.end(), svc);
    if (it == serviceTypes.end()) return false;
    serviceTypes.erase(it);
    return true;
}

// 打印好友信息
void Friend_Hejr::printInfo() const
{
    cout << "  好友账号号: " << friendId
         << "  备注: " << remark
         << "  来源服务: " << getServiceType() << endl;
}

