#include "Group_Hejr.h"
#include <iostream>
#include <algorithm>
using namespace std;

// 构造：群主自动成为第一个群成员
Group_Hejr::Group_Hejr(string gid, string gname, string owner,
                   GroupStrategy_Hejr* strat)
{
    groupId   = gid;
    groupName = gname;
    ownerId   = owner;
    strategy  = strat;
    members.push_back(owner);
}

// 内部工具：判断是否管理员
bool Group_Hejr::isAdmin(string userId) const
{
    return find(admins.begin(), admins.end(), userId) != admins.end();
}

// 判断某人是否为群成员
bool Group_Hejr::isMember(string userId) const
{
    return find(members.begin(), members.end(), userId) != members.end();
}

// ---------- 申请加入 ----------
bool Group_Hejr::applyJoin(string userId)
{
    // 由当前策略决定是否允许申请（微信群模式拒绝）
    if (!strategy->canApplyJoin())
    {
        cout << "[群" << groupId << "] " << strategy->getStrategyName()
             << "不允许申请加入，请联系群成员邀请。" << endl;
        return false;
    }
    if (isMember(userId))
    {
        cout << "[群" << groupId << "] 用户 " << userId << " 已在群内。" << endl;
        return false;
    }

    members.push_back(userId);   // 课程设计简化：申请自动通过
    cout << "[群" << groupId << "] 用户 " << userId
         << " 申请加入成功（当前共 " << members.size() << " 人）。" << endl;
    return true;
}

// ---------- 推荐/邀请加入 ----------
bool Group_Hejr::inviteJoin(string userId, string inviterId)
{
    if (!strategy->canInviteJoin())
    {
        cout << "[群" << groupId << "] 当前模式不允许邀请加入。" << endl;
        return false;
    }
    // 邀请人必须已经是群成员
    if (!isMember(inviterId))
    {
        cout << "[群" << groupId << "] 邀请人 " << inviterId
             << " 不在群内，无法推荐。" << endl;
        return false;
    }
    if (isMember(userId))
    {
        cout << "[群" << groupId << "] 用户 " << userId << " 已在群内。" << endl;
        return false;
    }

    members.push_back(userId);
    cout << "[群" << groupId << "] 群成员 " << inviterId << " 邀请 "
         << userId << " 加入成功（当前共 " << members.size() << " 人）。" << endl;
    return true;
}

// ---------- 主动退群 ----------
bool Group_Hejr::quitGroup(string userId)
{
    if (userId == ownerId)
    {
        cout << "[群" << groupId << "] 群主不能退群（可解散群）。" << endl;
        return false;
    }
    vector<string>::iterator it =
        find(members.begin(), members.end(), userId);
    if (it == members.end())
    {
        cout << "[群" << groupId << "] 用户 " << userId << " 不在群内。" << endl;
        return false;
    }

    members.erase(it);
    // 同步从管理员、各子群中移除
    admins.erase(remove(admins.begin(), admins.end(), userId), admins.end());
    for (size_t i = 0; i < subGroups.size(); i++)
    {
        subGroups[i].members.erase(
            remove(subGroups[i].members.begin(),
                   subGroups[i].members.end(), userId),
            subGroups[i].members.end());
    }

    cout << "[群" << groupId << "] 用户 " << userId << " 已退群。" << endl;
    return true;
}

// ---------- 挨T ----------
bool Group_Hejr::kickMember(string operatorId, string targetId)
{
    if (!isMember(operatorId))
    {
        cout << "[群" << groupId << "] 操作者不在群内。" << endl;
        return false;
    }
    if (!isMember(targetId))
    {
        cout << "[群" << groupId << "] 目标用户不在群内。" << endl;
        return false;
    }

    bool allowed = false;

    if (operatorId == ownerId)
    {
        // 群主可以踢任何人（但不能踢自己）
        if (targetId != ownerId)
            allowed = true;
    }
    else if (strategy->hasAdminSystem() && isAdmin(operatorId))
    {
        // QQ群模式：管理员可踢普通成员，但不能踢群主或其他管理员
        if (targetId != ownerId && !isAdmin(targetId))
            allowed = true;
    }

    if (!allowed)
    {
        cout << "[群" << groupId << "] " << operatorId
             << " 无权将 " << targetId << " 移出群（"
             << strategy->getStrategyName() << "）。" << endl;
        return false;
    }

    members.erase(remove(members.begin(), members.end(), targetId),
                  members.end());
    admins.erase(remove(admins.begin(), admins.end(), targetId),
                 admins.end());
    for (size_t i = 0; i < subGroups.size(); i++)
    {
        subGroups[i].members.erase(
            remove(subGroups[i].members.begin(),
                   subGroups[i].members.end(), targetId),
            subGroups[i].members.end());
    }

    cout << "[群" << groupId << "] 用户 " << targetId
         << " 已被 " << operatorId << " 移出群聊。" << endl;
    return true;
}

// ---------- 添加管理员（仅群主，仅QQ群模式） ----------
bool Group_Hejr::addAdmin(string operatorId, string userId)
{
    if (operatorId != ownerId)
    {
        cout << "[群" << groupId << "] 只有群主可以设置管理员。" << endl;
        return false;
    }
    if (!strategy->hasAdminSystem())
    {
        cout << "[群" << groupId << "] " << strategy->getStrategyName()
             << "没有管理员制度，仅有群主为特权账号。" << endl;
        return false;
    }
    if (!isMember(userId))
    {
        cout << "[群" << groupId << "] 用户 " << userId << " 不在群内。" << endl;
        return false;
    }
    if (isAdmin(userId))
    {
        cout << "[群" << groupId << "] 用户 " << userId << " 已是管理员。" << endl;
        return false;
    }

    admins.push_back(userId);
    cout << "[群" << groupId << "] 已设置 " << userId << " 为管理员。" << endl;
    return true;
}

// ---------- 移除管理员 ----------
bool Group_Hejr::removeAdmin(string operatorId, string userId)
{
    if (operatorId != ownerId)
    {
        cout << "[群" << groupId << "] 只有群主可以移除管理员。" << endl;
        return false;
    }
    vector<string>::iterator it =
        find(admins.begin(), admins.end(), userId);
    if (it == admins.end())
    {
        cout << "[群" << groupId << "] 用户 " << userId << " 不是管理员。" << endl;
        return false;
    }

    admins.erase(it);
    cout << "[群" << groupId << "] 已取消 " << userId << " 的管理员身份。" << endl;
    return true;
}

// ---------- 创建临时讨论组（子群，仅QQ群模式） ----------
bool Group_Hejr::createSubgroup(string name, string creatorId)
{
    if (!isMember(creatorId))
    {
        cout << "[群" << groupId << "] 非群成员不能创建讨论组。" << endl;
        return false;
    }
    if (!strategy->canCreateSubgroup())
    {
        cout << "[群" << groupId << "] " << strategy->getStrategyName()
             << "不允许创建临时讨论组（子群）。" << endl;
        return false;
    }
    for (size_t i = 0; i < subGroups.size(); i++)
    {
        if (subGroups[i].name == name)
        {
            cout << "[群" << groupId << "] 讨论组 " << name
                 << " 已存在。" << endl;
            return false;
        }
    }

    SubGroup_Hejr sg;
    sg.name = name;
    sg.members.push_back(creatorId);
    subGroups.push_back(sg);

    cout << "[群" << groupId << "] 已创建临时讨论组「" << name
         << "」，创建者 " << creatorId << "。" << endl;
    return true;
}

// ---------- 列出所有子群 ----------
void Group_Hejr::listSubgroups() const
{
    cout << "[群" << groupId << "] 临时讨论组列表（共 "
         << subGroups.size() << " 个）：" << endl;
    for (size_t i = 0; i < subGroups.size(); i++)
    {
        cout << "  讨论组: " << subGroups[i].name << "  成员: ";
        for (size_t j = 0; j < subGroups[i].members.size(); j++)
            cout << subGroups[i].members[j] << " ";
        cout << endl;
    }
}

// ---------- 列出群成员 ----------
void Group_Hejr::listMembers() const
{
    cout << "========== 群 " << groupId << " (" << groupName
         << ") 成员列表 ==========" << endl;
    cout << "管理模式: " << strategy->getStrategyName() << endl;
    for (size_t i = 0; i < members.size(); i++)
    {
        cout << "  " << members[i];
        if (members[i] == ownerId)
            cout << "  [群主]";
        // 管理员标记仅在当前策略存在管理员制度时显示；
        // 切到微信群模式后管理员数据仍保留，但不再享有特权（特权随策略动态生效）
        else if (strategy->hasAdminSystem() && isAdmin(members[i]))
            cout << "  [管理员]";
        cout << endl;
    }
    cout << "共 " << members.size() << " 人" << endl;
    cout << "=======================================" << endl;
}

// ---------- 展示当前群的管理特色 ----------
void Group_Hejr::showFeature() const
{
    cout << "========== 群 " << groupId << " (" << groupName
         << ") 当前特色 ==========" << endl;
    cout << "管理模式:   " << strategy->getStrategyName() << endl;
    cout << "申请加入:   " << (strategy->canApplyJoin()    ? "支持" : "不支持") << endl;
    cout << "邀请加入:   " << (strategy->canInviteJoin()   ? "支持" : "不支持") << endl;
    cout << "临时讨论组: " << (strategy->canCreateSubgroup() ? "支持" : "不支持") << endl;
    cout << "管理员制度: " << (strategy->hasAdminSystem()  ? "群主+管理员" : "仅群主")
         << endl;
    cout << "群成员数:   " << members.size()
         << "（成员数据不受模式变换影响）" << endl;
    cout << "=====================================" << endl;
}

// ---------- 动态切换管理模式（成员数据不变） ----------
void Group_Hejr::setStrategy(GroupStrategy_Hejr* strat)
{
    string oldName = strategy->getStrategyName();
    strategy = strat;
    cout << "[群" << groupId << "] 管理模式已由「" << oldName
         << "」动态变换为「" << strategy->getStrategyName()
         << "」，" << members.size() << " 名成员数据保持不变。" << endl;
    // 说明：admins 向量保留不删除——切回QQ群模式时管理员设置依然有效；
    //       在微信群模式下 addAdmin/kickMember 等操作会被策略拒绝。
}

// ---------- getter ----------
string Group_Hejr::getGroupId() const       { return groupId; }
string Group_Hejr::getGroupName() const     { return groupName; }
string Group_Hejr::getOwnerId() const       { return ownerId; }
void Group_Hejr::setOwnerId(string userId)
{
    // 仅用于旧数据迁移时，移除早期写入的占位群主。
    if (ownerId == "system")
        members.erase(remove(members.begin(), members.end(), ownerId), members.end());
    ownerId = userId;
    loadMember(userId);
}
string Group_Hejr::getStrategyName() const  { return strategy->getStrategyName(); }

vector<string> Group_Hejr::getMembers() const { return members; }
vector<string> Group_Hejr::getAdmins() const  { return admins; }

// ---------- 文件I/O辅助方法（绕过策略校验，仅用于反序列化） ----------

// 直接添加成员（不经过申请/邀请流程，用于从文件恢复数据）
void Group_Hejr::loadMember(string userId)
{
    if (!isMember(userId))
        members.push_back(userId);
}

// 直接添加管理员（不经过群主校验，用于从文件恢复数据）
void Group_Hejr::loadAdmin(string userId)
{
    if (!isAdmin(userId))
        admins.push_back(userId);
}

// 获取子群列表
vector<SubGroup_Hejr> Group_Hejr::getSubGroups() const
{
    return subGroups;
}

// 直接加载子群（用于从文件恢复数据）
void Group_Hejr::loadSubGroup(string name, vector<string> mems)
{
    SubGroup_Hejr sg;
    sg.name = name;
    sg.members = mems;
    subGroups.push_back(sg);
}

