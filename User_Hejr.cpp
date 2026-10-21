#include "User_Hejr.h"
#include <iostream>
#include <ctime>
#include <algorithm>
using namespace std;

// 默认构造
User_Hejr::User_Hejr()
{
    accountNumber = "";
    id           = "";
    nickname     = "";
    wechatId     = "";
    wechatIdChangedMonth = "";
    wechatBoundQQ = "";
    birthday     = "1970-01-01";
    registerTime = "2000-01-01";
    location     = "";
    password     = "123456";
}

// 带参构造
User_Hejr::User_Hejr(string uid, string nick, string birth,
                 string regTime, string loc)
{
    accountNumber = uid;
    id           = nick;
    nickname     = nick;
    wechatId     = "";
    wechatIdChangedMonth = "";
    wechatBoundQQ = "";
    birthday     = birth;
    registerTime = regTime;
    location     = loc;
    password     = "123456";
}

// ---- 基本信息接口 ----
string User_Hejr::getId() const          { return id; }
void   User_Hejr::setId(string uid)      { id = uid; nickname = uid; }

string User_Hejr::getAccountNumber() const { return accountNumber; }
void   User_Hejr::setAccountNumber(string account) { accountNumber = account; }

string User_Hejr::getNickname() const    { return nickname; }
void   User_Hejr::setNickname(string n)  { nickname = n; id = n; }

string User_Hejr::getWeChatId() const { return wechatId; }
void   User_Hejr::setWeChatId(string value) { wechatId = value; }
string User_Hejr::getWeChatIdChangedMonth() const { return wechatIdChangedMonth; }
void   User_Hejr::setWeChatIdChangedMonth(string month) { wechatIdChangedMonth = month; }
string User_Hejr::getWeChatBoundQQ() const { return wechatBoundQQ; }
void   User_Hejr::setWeChatBoundQQ(string account) { wechatBoundQQ = account; }

string User_Hejr::getBirthday() const    { return birthday; }
void   User_Hejr::setBirthday(string b)  { birthday = b; }

string User_Hejr::getRegisterTime() const { return registerTime; }
void   User_Hejr::setRegisterTime(string r) { registerTime = r; }

string User_Hejr::getLocation() const    { return location; }
void   User_Hejr::setLocation(string l)  { location = l; }

bool User_Hejr::checkPassword(string candidate) const
{
    return password == candidate;
}

void User_Hejr::changePassword(string newPassword)
{
    password = newPassword;
}

string User_Hejr::getPassword() const
{
    return password;
}

// 计算 T龄：当前年份 - 注册年份
int User_Hejr::getTAge() const
{
    // 取当前年份
    time_t now = time(0);
    tm* ltm = localtime(&now);
    int curYear = 1900 + ltm->tm_year;

    // 从 registerTime 中解析年份（格式 YYYY-MM-DD）
    int regYear = 2000;
    if (registerTime.size() >= 4)
    {
        regYear = atoi(registerTime.substr(0, 4).c_str());
    }

    int age = curYear - regYear;
    if (registerTime.size() >= 10)
    {
        int regMonth = atoi(registerTime.substr(5, 2).c_str());
        int regDay = atoi(registerTime.substr(8, 2).c_str());
        if (ltm->tm_mon + 1 < regMonth ||
            (ltm->tm_mon + 1 == regMonth && ltm->tm_mday < regDay))
            --age;
    }
    return age < 0 ? 0 : age;
}

// ---- 开通服务管理 ----
void User_Hejr::addService(string serviceType)
{
    if (!isServiceOpened(serviceType))
        openedServices.push_back(serviceType);
}

void User_Hejr::removeService(string serviceType)
{
    vector<string>::iterator it =
        find(openedServices.begin(), openedServices.end(), serviceType);
    if (it != openedServices.end())
        openedServices.erase(it);
}

bool User_Hejr::isServiceOpened(string serviceType) const
{
    return find(openedServices.begin(), openedServices.end(),
                serviceType) != openedServices.end();
}

vector<string> User_Hejr::getOpenedServices() const
{
    return openedServices;
}

// ---- 好友ID管理 ----
void User_Hejr::addFriendId(string fid)
{
    if (find(friendIds.begin(), friendIds.end(), fid) == friendIds.end())
        friendIds.push_back(fid);
}

void User_Hejr::removeFriendId(string fid)
{
    vector<string>::iterator it =
        find(friendIds.begin(), friendIds.end(), fid);
    if (it != friendIds.end())
        friendIds.erase(it);
}

vector<string> User_Hejr::getFriendIds() const
{
    return friendIds;
}

// ---- 群ID管理 ----
void User_Hejr::addGroupId(string gid)
{
    if (find(groupIds.begin(), groupIds.end(), gid) == groupIds.end())
        groupIds.push_back(gid);
}

void User_Hejr::removeGroupId(string gid)
{
    vector<string>::iterator it =
        find(groupIds.begin(), groupIds.end(), gid);
    if (it != groupIds.end())
        groupIds.erase(it);
}

vector<string> User_Hejr::getGroupIds() const
{
    return groupIds;
}

// 打印用户全部信息
void User_Hejr::printInfo() const
{
    cout << "========== 用户信息 ==========" << endl;
    cout << "平台账号号:" << accountNumber << endl;
    cout << "昵称:      " << nickname << endl;
    if (isServiceOpened("QQ")) cout << "QQ账号号:  " << accountNumber << endl;
    if (isServiceOpened("Weibo")) cout << "微博账号号:" << accountNumber << endl;
    if (isServiceOpened("WeChat"))
    {
        cout << "微信账号号:" << wechatId << endl;
        cout << "绑定QQ:    " << (wechatBoundQQ.empty() ? "未绑定" : wechatBoundQQ) << endl;
    }
    cout << "出生时间:  " << (birthday.empty() ? "未填写" : birthday) << endl;
    cout << "注册时间:  " << registerTime << "  (T龄: " << getTAge() << " 年)" << endl;
    cout << "所在地:    " << (location.empty() ? "未填写" : location) << endl;

    cout << "已开通服务: ";
    for (size_t i = 0; i < openedServices.size(); i++)
        cout << openedServices[i] << " ";
    cout << endl;

    cout << "好友账号号:";
    for (size_t i = 0; i < friendIds.size(); i++)
        cout << friendIds[i] << " ";
    cout << endl;

    cout << "群ID:      ";
    for (size_t i = 0; i < groupIds.size(); i++)
        cout << groupIds[i] << " ";
    cout << endl;
    cout << "==============================" << endl;
}

