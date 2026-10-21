#ifndef GROUP_HEJR_H
#define GROUP_HEJR_H

#include <string>
#include <vector>
#include "GroupStrategy_Hejr.h"
using namespace std;

// ============================================================
// 群信息类 Group_Hejr
// 说明：
//   - 群的共性数据（群号、群名、群主、成员、管理员、子群）与
//     "可变的管理模式"（GroupStrategy_Hejr）通过组合分离；
//   - 调用 setStrategy() 可在不改变成员数据的前提下，
//     动态把群从 QQ群模式 切换为 微信群模式（或反之）；
//   - 本类不负责释放 strategy 指针（策略对象通常由外部统一创建、
//     多个群共享），避免重复释放。
//   - 类名后缀 _Hejr 为姓名缩写占位符，请替换。
// ============================================================

// 临时讨论组（子群），仅 QQ群模式允许创建
struct SubGroup_Hejr
{
    string name;              // 讨论组名称
    vector<string> members;   // 讨论组成员（须为群成员）
};

class Group_Hejr
{
private:
    string serviceType;        // 所属服务：QQ 或 WeChat，切换管理策略时保持不变
    string groupId;            // 群号（如 1001~1006）
    string legacyGroupId;      // 迁移前的纯数字群号，用于读取原有群聊记录
    string groupName;          // 群名称
    string ownerId;            // 群主ID
    vector<string> members;    // 按加入先后保存成员；转让群主不改变顺序
    vector<string> admins;     // 管理员ID列表（仅QQ群模式有效）
    vector<SubGroup_Hejr> subGroups; // 临时讨论组（子群）
    GroupStrategy_Hejr* strategy;    // 当前群管理策略（组合）

    // 内部工具：判断某人是否为管理员
    bool isAdmin(string userId) const;

public:
    // 构造：建群时指定群号、群名、群主、初始策略，群主自动入群
    Group_Hejr(string service, string gid, string gname, string owner,
             GroupStrategy_Hejr* strat);

    // ---------- 入群 / 退群 / 踢人 ----------
    // 申请加入（QQ群模式允许，微信群模式拒绝）
    bool applyJoin(string userId);

    // 推荐/邀请加入：inviterId 必须是本群成员（两种模式均支持）
    bool inviteJoin(string userId, string inviterId);

    // 主动退群；返回成功后，空成员表表示需要由群容器删除该群
    bool quitGroup(string userId);
    bool transferOwnership(string operatorId, string targetId);

    // 挨T：操作者须为群主；QQ群模式下管理员也可踢普通成员
    bool kickMember(string operatorId, string targetId);

    // ---------- 管理员制度（仅QQ群模式有意义） ----------
    bool addAdmin(string operatorId, string userId);
    bool removeAdmin(string operatorId, string userId);

    // ---------- 临时讨论组（子群） ----------
    bool createSubgroup(string name, string creatorId);
    bool addSubgroupMember(string name, string operatorId, string userId);
    bool removeSubgroupMember(string name, string operatorId, string userId);
    void listSubgroups() const;

    // ---------- 查询 / 展示 ----------
    void   listMembers() const;
    bool   isMember(string userId) const;
    void   showFeature() const;   // 展示当前群管理特色

    // ---------- 核心：动态切换管理模式，成员数据不变 ----------
    void setStrategy(GroupStrategy_Hejr* strat);

    // ---------- 文件I/O辅助：直接加载成员/管理员（绕过策略校验，仅用于反序列化） ----------
    void loadMember(string userId);     // 直接添加成员（不经过申请/邀请流程）
    void loadMembers(const vector<string>& userIds); // 恢复完整加入顺序
    void loadAdmin(string userId);      // 直接添加管理员
    vector<SubGroup_Hejr> getSubGroups() const;  // 获取子群列表
    void loadSubGroup(string name, vector<string> mems);  // 直接加载子群

    // ---------- getter ----------
    string getGroupId() const;
    string getServiceType() const;
    string getStorageId() const; // 服务:群号，用于持久化和唯一定位
    string getLegacyGroupId() const;
    void setLegacyGroupId(string id);
    string getGroupName() const;
    string getOwnerId() const;
    void setOwnerId(string userId);
    string getStrategyName() const;
    vector<string> getMembers() const;
    vector<string> getAdmins() const;
};

#endif // GROUP_HEJR_H


