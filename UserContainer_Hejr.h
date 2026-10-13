#ifndef USERCONTAINER_HEJR_H
#define USERCONTAINER_HEJR_H

#include <vector>
#include "User_Hejr.h"
using namespace std;

// ============================================================
// 用户容器类 UserContainer_Hejr
// 职责：管理所有 User_Hejr 对象
// 持久化：构造时从 users.txt 加载，析构时写回 users.txt（断电保存）
// 文件格式：每行一条记录，字段以 | 分隔
//   accountNumber|nickname|birthday|registerTime|location|
//   openedServices|friendIds|groupIds|password|wechatId|wechatIdChangedMonth|wechatBoundQQ
// 类名后缀 _Hejr 为姓名缩写占位符，请替换。
// ============================================================
class UserContainer_Hejr
{
private:
    vector<User_Hejr> users;   // 用户列表
    string filename;          // 数据文件名

public:
    // 构造：指定文件名，自动加载
    UserContainer_Hejr(string fname = "users.txt");

    // 析构：自动保存
    ~UserContainer_Hejr();

    // ---------- 增删查 ----------
    void addUser(const User_Hejr& user);
    void removeUser(string userId);
    User_Hejr* findUserById(string accountNumber);   // 按不可变帐号号查找
    User_Hejr* findUserByWeChatId(string wechatId);
    bool isAccountNumberUsed(string accountNumber) const;
    bool isWeChatIdUsed(string wechatId, string exceptAccount = "") const;
    bool ensureWeChatId(User_Hejr& user);

    // 获取所有用户
    vector<User_Hejr>& getAllUsers();

    // 打印所有用户
    void listAll() const;

    // ---------- 文件 I/O ----------
    void load(string fname);
    void save(string fname) const;
    void save() const;
};

#endif // USERCONTAINER_HEJR_H


