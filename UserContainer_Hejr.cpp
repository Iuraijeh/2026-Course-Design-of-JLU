#include "UserContainer_Hejr.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
using namespace std;

static string normalizeStoredAccount(const string& account)
{
    string normalized = account;
    for (size_t i = 0; i < normalized.size(); ++i)
        normalized[i] = (char)tolower((unsigned char)normalized[i]);
    return normalized;
}

static string generatedWeChatId(const string& account, unsigned int retry)
{
    string source = account;
    if (retry > 0) source += "#" + to_string(retry);
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < source.size(); ++i)
    {
        hash ^= (unsigned char)source[i];
        hash *= UINT64_C(1099511628211);
    }
    ostringstream result;
    result << "wx_" << hex << nouppercase << setw(16) << setfill('0') << hash;
    return result.str();
}

// ---------- 辅助函数：逗号分隔字符串拆分为 vector ----------
static vector<string> splitComma(const string& s)
{
    vector<string> result;
    stringstream ss(s);
    string token;
    while (getline(ss, token, ','))
    {
        if (!token.empty())
            result.push_back(token);
    }
    return result;
}

// ---------- 辅助函数：vector<string> 合并为逗号分隔字符串 ----------
static string joinComma(const vector<string>& v)
{
    string result;
    for (size_t i = 0; i < v.size(); i++)
    {
        if (i > 0) result += ",";
        result += v[i];
    }
    return result;
}

// ---------- 构造：自动加载 ----------
UserContainer_Hejr::UserContainer_Hejr(string fname)
{
    filename = fname;
    load(filename);
}

// 写操作由平台立即保存；析构不重写可能已被网页更新的文件。
UserContainer_Hejr::~UserContainer_Hejr()
{
}

// ---------- 添加用户 ----------
void UserContainer_Hejr::addUser(const User_Hejr& user)
{
    // 避免重复添加
    if (findUserById(user.getAccountNumber()) != NULL ||
        isWeChatIdUsed(user.getAccountNumber()))
    {
        cout << "[用户容器] 账号号 " << user.getAccountNumber() << " 已被使用。" << endl;
        return;
    }
    users.push_back(user);
    if (users.back().isServiceOpened("WeChat")) ensureWeChatId(users.back());
    cout << "[用户容器] 已添加帐号: " << user.getAccountNumber() << endl;
}

// ---------- 删除用户 ----------
void UserContainer_Hejr::removeUser(string userId)
{
    for (size_t i = 0; i < users.size(); i++)
    {
        if (normalizeStoredAccount(users[i].getAccountNumber()) ==
            normalizeStoredAccount(userId))
        {
            users.erase(users.begin() + i);
            cout << "[用户容器] 已删除用户: " << userId << endl;
            return;
        }
    }
    cout << "[用户容器] 用户 " << userId << " 不存在。" << endl;
}

// ---------- 按ID查找 ----------
User_Hejr* UserContainer_Hejr::findUserById(string userId)
{
    for (size_t i = 0; i < users.size(); i++)
    {
        if (normalizeStoredAccount(users[i].getAccountNumber()) ==
            normalizeStoredAccount(userId))
            return &users[i];
    }
    return NULL;
}

User_Hejr* UserContainer_Hejr::findUserByWeChatId(string wechatId)
{
    string key = normalizeStoredAccount(wechatId);
    for (size_t i = 0; i < users.size(); ++i)
    {
        if (users[i].isServiceOpened("WeChat") &&
            normalizeStoredAccount(users[i].getWeChatId()) == key)
            return &users[i];
    }
    return NULL;
}

bool UserContainer_Hejr::isWeChatIdUsed(string wechatId, string exceptAccount) const
{
    string key = normalizeStoredAccount(wechatId);
    string except = normalizeStoredAccount(exceptAccount);
    for (size_t i = 0; i < users.size(); ++i)
    {
        const User_Hejr& user = users[i];
        // 微信账号号不可与任何平台账号号冲突。
        if (normalizeStoredAccount(user.getAccountNumber()) == key) return true;
        if (normalizeStoredAccount(user.getAccountNumber()) != except &&
            !user.getWeChatId().empty() &&
            normalizeStoredAccount(user.getWeChatId()) == key)
            return true;
    }
    return false;
}

bool UserContainer_Hejr::ensureWeChatId(User_Hejr& user)
{
    if (!user.isServiceOpened("WeChat") || !user.getWeChatId().empty()) return false;
    for (unsigned int retry = 0; retry < 1000000; ++retry)
    {
        string candidate = generatedWeChatId(user.getAccountNumber(), retry);
        if (!isWeChatIdUsed(candidate, user.getAccountNumber()))
        {
            user.setWeChatId(candidate);
            return true;
        }
    }
    return false;
}

bool UserContainer_Hejr::isAccountNumberUsed(string accountNumber) const
{
    for (size_t i = 0; i < users.size(); ++i)
    {
        if (normalizeStoredAccount(users[i].getAccountNumber()) ==
            normalizeStoredAccount(accountNumber))
            return true;
    }
    return false;
}

// ---------- 获取所有用户 ----------
vector<User_Hejr>& UserContainer_Hejr::getAllUsers()
{
    return users;
}

// ---------- 打印所有用户 ----------
void UserContainer_Hejr::listAll() const
{
    cout << "========== 用户列表（共 " << users.size() << " 人）==========" << endl;
    for (size_t i = 0; i < users.size(); i++)
    {
        users[i].printInfo();
    }
}

// ---------- 从文件加载 ----------
void UserContainer_Hejr::load(string fname)
{
    ifstream fin(fname.c_str());
    if (!fin.is_open())
    {
        cout << "[用户容器] 文件 " << fname << " 不存在，将首次运行时创建。" << endl;
        return;
    }

    users.clear();
    string line;
    while (getline(fin, line))
    {
        if (line.empty()) continue;

        // 按 | 分隔字段
        // 格式: accountNumber|nickname|birthday|registerTime|location|openedServices|friendIds|groupIds|password|wechatId|changedMonth|boundQQ
        vector<string> fields;
        stringstream ss(line);
        string field;
        while (getline(ss, field, '|'))
            fields.push_back(field);

        if (fields.size() < 5) continue;

        User_Hejr u;
        u.setAccountNumber(fields[0]);
        u.setId(fields[1]);
        u.setBirthday(fields[2]);
        u.setRegisterTime(fields[3]);
        u.setLocation(fields[4]);

        // 已开通服务
        if (fields.size() > 5)
        {
            vector<string> svcs = splitComma(fields[5]);
            for (size_t i = 0; i < svcs.size(); i++)
                u.addService(svcs[i]);
        }

        // 好友ID
        if (fields.size() > 6)
        {
            vector<string> fids = splitComma(fields[6]);
            for (size_t i = 0; i < fids.size(); i++)
                u.addFriendId(fids[i]);
        }

        // 群ID
        if (fields.size() > 7)
        {
            vector<string> gids = splitComma(fields[7]);
            for (size_t i = 0; i < gids.size(); i++)
                u.addGroupId(gids[i]);
        }

        // 第 9 列为平台登录密码；旧文件缺少后续微信字段时仍可读取。
        if (fields.size() > 8 && !fields[8].empty())
            u.changePassword(fields[8]);

        if (fields.size() > 9) u.setWeChatId(fields[9]);
        if (fields.size() > 10) u.setWeChatIdChangedMonth(fields[10]);
        if (fields.size() > 11) u.setWeChatBoundQQ(fields[11]);

        users.push_back(u);
    }
    fin.close();

    for (size_t i = 0; i < users.size(); ++i)
        ensureWeChatId(users[i]);

    cout << "[用户容器] 已从 " << fname << " 加载 "
         << users.size() << " 个用户。" << endl;
}

// ---------- 保存到文件 ----------
void UserContainer_Hejr::save(string fname) const
{
    ofstream fout(fname.c_str());
    if (!fout.is_open())
    {
        cerr << "[用户容器] 无法写入文件 " << fname << "！" << endl;
        return;
    }

    for (size_t i = 0; i < users.size(); i++)
    {
        const User_Hejr& u = users[i];
        fout << u.getAccountNumber()   << "|"
             << u.getId()              << "|"
             << u.getBirthday()        << "|"
             << u.getRegisterTime()    << "|"
             << u.getLocation()        << "|"
             << joinComma(u.getOpenedServices()) << "|"
             << joinComma(u.getFriendIds())      << "|"
             << joinComma(u.getGroupIds())       << "|"
             << u.getPassword()                  << "|"
             << u.getWeChatId()                  << "|"
             << u.getWeChatIdChangedMonth()      << "|"
             << u.getWeChatBoundQQ()             << endl;
    }
    fout.close();

    cout << "[用户容器] 已保存 " << users.size() << " 个用户到 "
         << fname << "。" << endl;
}

void UserContainer_Hejr::save() const
{
    save(filename);
}

