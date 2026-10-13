#ifndef WECHATGROUPSTRATEGY_HEJR_H
#define WECHATGROUPSTRATEGY_HEJR_H

#include "GroupStrategy_Hejr.h"
#include <string>
using namespace std;

// ============================================================
// 微信群管理策略 WeChatGroupStrategy_Hejr
// 特点：
//   1. 不能申请加入，只能由群内成员推荐/邀请加入；
//   2. 不允许设置临时讨论组（子群）；
//   3. 没有管理员制度，仅有群主为特权账号。
// ============================================================
class WeChatGroupStrategy_Hejr : public GroupStrategy_Hejr
{
public:
    virtual bool   canApplyJoin();
    virtual bool   canInviteJoin();
    virtual bool   canCreateSubgroup();
    virtual bool   hasAdminSystem();
    virtual string getStrategyName();
};

#endif // WECHATGROUPSTRATEGY_HEJR_H


