#ifndef FRIENDCONTAINER_HEJR_H
#define FRIENDCONTAINER_HEJR_H

#include <vector>
#include <string>
#include "Friend_Hejr.h"
using namespace std;

// ============================================================
// 好友容器类 FriendContainer_Hejr
// 职责：管理某用户的好友列表（Friend_Hejr 对象）
// 持久化：构造时从 friends_<userId>.txt 加载，析构时写回
// 文件格式：每行一条记录，字段以 | 分隔
//   friendId|remark|serviceType
// 类名后缀 _Hejr 为姓名缩写占位符，请替换。
// ============================================================
class FriendContainer_Hejr
{
private:
    string userId;              // 所属用户ID
    vector<Friend_Hejr> friends;   // 好友列表
    string filename;             // 数据文件名

    // 根据用户ID生成文件名
    string makeFilename(string uid);

public:
    // 构造：传入用户ID，自动加载该用户的好友文件
    FriendContainer_Hejr(string uid);

    // 析构：自动保存
    ~FriendContainer_Hejr();

    // ---------- 增删改查 ----------
    bool addFriend(const Friend_Hejr& f);
    bool removeFriend(string friendId);
    bool modifyFriend(string friendId, string newRemark, string newServiceType);
    Friend_Hejr* findFriend(string friendId);   // 返回指针，未找到返回 NULL

    // 获取所有好友
    vector<Friend_Hejr>& getAllFriends();

    // 打印好友列表
    void listAll() const;

    // ---------- 文件 I/O ----------
    void load();
    void save() const;
};

#endif // FRIENDCONTAINER_HEJR_H


