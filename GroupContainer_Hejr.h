#ifndef GROUPCONTAINER_HEJR_H
#define GROUPCONTAINER_HEJR_H

#include <vector>
#include "Group_Hejr.h"
#include "GroupStrategy_Hejr.h"
#include "QQGroupStrategy_Hejr.h"
#include "WeChatGroupStrategy_Hejr.h"
using namespace std;

// ============================================================
// 群容器类 GroupContainer_Hejr
// 职责：管理所有群（Group_Hejr* 指针）
// 持久化：构造时加载，修改成功后由平台立即保存。
//         QQ 和微信各预置 1001~1006 群
// 文件格式：每行一条记录，字段以 | 分隔
//   首行 #group-state-v1|下一个QQ群号|下一个微信群号
//   serviceType:groupId|groupName|ownerId|strategyType|
//   members(逗号分隔)|admins(逗号分隔)|
//   subgroups(分号分隔,每个: name:成员逗号)
// 策略对象由容器统一创建、管理、释放。
// 类名后缀 _Hejr 为姓名缩写占位符，请替换。
// ============================================================
class GroupContainer_Hejr
{
private:
    vector<Group_Hejr*> groups;       // 群指针列表
    QQGroupStrategy_Hejr*     qqStrategy;   // QQ群策略（共享）
    WeChatGroupStrategy_Hejr* wxStrategy;   // 微信群策略（共享）

    string filename;                 // 数据文件名
    int nextQQId;
    int nextWeChatId;

    // 创建旧版六个预设群，并补齐两个服务各 1001~1006
    void initDefaultGroups();
    bool ensureDefaultGroups();

public:
    // 根据类型字符串获取策略指针（public，供 Platform 动态切换群模式）
    GroupStrategy_Hejr* getStrategy(string type);

    // 构造：自动加载或初始化
    GroupContainer_Hejr(string fname = "groups.txt");

    // 析构：保存并释放内存
    ~GroupContainer_Hejr();

    // ---------- 查询 ----------
    Group_Hejr* findByGroupId(string storageId);  // 如 QQ:1001
    vector<Group_Hejr*> getAllGroups();
    void listAll() const;

    // ---------- 添加群 ----------
    void addGroup(Group_Hejr* g);
    Group_Hejr* createGroup(string service, string name, string owner,
                           const vector<string>& selectedMembers);
    bool removeGroup(string storageId);

    // ---------- 文件 I/O ----------
    void load(string fname);
    void save(string fname) const;
};

#endif // GROUPCONTAINER_HEJR_H


