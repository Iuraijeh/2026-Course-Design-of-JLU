#include "FriendContainer_Hejr.h"
#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

// 根据用户ID生成好友文件名：friends_<userId>.txt
string FriendContainer_Hejr::makeFilename(string uid)
{
    return "friends_" + uid + ".txt";
}

// ---------- 构造：自动加载 ----------
FriendContainer_Hejr::FriendContainer_Hejr(string uid)
{
    userId   = uid;
    filename = makeFilename(uid);
    load();
}

// 写操作由平台立即保存；析构不重写可能已被网页更新的文件。
FriendContainer_Hejr::~FriendContainer_Hejr()
{
}

// ---------- 添加好友 ----------
bool FriendContainer_Hejr::addFriend(const Friend_Hejr& f)
{
    Friend_Hejr* existing = findFriend(f.getFriendId());
    if (existing != NULL)
    {
        bool added = false;
        vector<string> types = f.getServiceTypes();
        for (size_t i = 0; i < types.size(); ++i)
            added = existing->addServiceType(types[i]) || added;
        if (!added)
            cout << "[好友容器] 好友 " << f.getFriendId() << " 已存在于该服务。" << endl;
        return added;
    }
    friends.push_back(f);
    cout << "[好友容器] 已添加好友: " << f.getFriendId()
         << "（用户 " << userId << "）" << endl;
    return true;
}

// ---------- 删除好友 ----------
bool FriendContainer_Hejr::removeFriend(string friendId)
{
    for (size_t i = 0; i < friends.size(); i++)
    {
        if (friends[i].getFriendId() == friendId)
        {
            friends.erase(friends.begin() + i);
            cout << "[好友容器] 已删除好友: " << friendId
                 << "（用户 " << userId << "）" << endl;
            return true;
        }
    }
    cout << "[好友容器] 好友 " << friendId << " 不存在。" << endl;
    return false;
}

// ---------- 修改好友信息 ----------
bool FriendContainer_Hejr::modifyFriend(string friendId,
                                       string newRemark,
                                       string newServiceType)
{
    Friend_Hejr* f = findFriend(friendId);
    if (f == NULL)
    {
        cout << "[好友容器] 好友 " << friendId << " 不存在，无法修改。" << endl;
        return false;
    }
    f->setRemark(newRemark);
    f->setServiceType(newServiceType);
    cout << "[好友容器] 已修改好友 " << friendId << " 的信息。" << endl;
    return true;
}

// ---------- 查找好友 ----------
Friend_Hejr* FriendContainer_Hejr::findFriend(string friendId)
{
    for (size_t i = 0; i < friends.size(); i++)
    {
        if (friends[i].getFriendId() == friendId)
            return &friends[i];
    }
    return NULL;
}

// ---------- 获取所有好友 ----------
vector<Friend_Hejr>& FriendContainer_Hejr::getAllFriends()
{
    return friends;
}

// ---------- 打印好友列表 ----------
void FriendContainer_Hejr::listAll() const
{
    cout << "========== 用户 " << userId << " 的好友列表（共 "
         << friends.size() << " 人）==========" << endl;
    for (size_t i = 0; i < friends.size(); i++)
    {
        friends[i].printInfo();
    }
    if (friends.empty())
        cout << "  （暂无好友）" << endl;
    cout << "==================================" << endl;
}

// ---------- 从文件加载 ----------
void FriendContainer_Hejr::load()
{
    ifstream fin(filename.c_str());
    if (!fin.is_open())
    {
        cout << "[好友容器] 文件 " << filename
             << " 不存在，将首次运行时创建。" << endl;
        return;
    }

    friends.clear();
    string line;
    while (getline(fin, line))
    {
        if (line.empty()) continue;

        // 格式: friendId|remark|serviceType
        stringstream ss(line);
        string field;
        vector<string> fields;
        while (getline(ss, field, '|'))
            fields.push_back(field);

        if (fields.size() >= 3) addFriend(Friend_Hejr(fields[0], fields[1], fields[2]));
    }
    fin.close();

    cout << "[好友容器] 已从 " << filename << " 加载 "
         << friends.size() << " 个好友。" << endl;
}

// ---------- 保存到文件 ----------
void FriendContainer_Hejr::save() const
{
    ofstream fout(filename.c_str());
    if (!fout.is_open())
    {
        cerr << "[好友容器] 无法写入文件 " << filename << "！" << endl;
        return;
    }

    for (size_t i = 0; i < friends.size(); i++)
    {
        fout << friends[i].getFriendId()  << "|"
             << friends[i].getRemark()     << "|"
             << friends[i].getServiceType() << endl;
    }
    fout.close();

    cout << "[好友容器] 已保存 " << friends.size() << " 个好友到 "
         << filename << "。" << endl;
}

