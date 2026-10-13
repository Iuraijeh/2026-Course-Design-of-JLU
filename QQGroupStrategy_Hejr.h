#ifndef QQGROUPSTRATEGY_HEJR_H
#define QQGROUPSTRATEGY_HEJR_H

#include "GroupStrategy_Hejr.h"
#include <string>
using namespace std;

// ============================================================
// QQ群管理策略 QQGroupStrategy_Hejr
// 特点：
//   1. 可以申请加入；
//   2. 也可以邀请加入；
//   3. 允许设置临时讨论组（子群）；
//   4. 有以群主为核心的管理员制度。
// ============================================================
class QQGroupStrategy_Hejr : public GroupStrategy_Hejr
{
public:
    virtual bool   canApplyJoin();
    virtual bool   canInviteJoin();
    virtual bool   canCreateSubgroup();
    virtual bool   hasAdminSystem();
    virtual string getStrategyName();
};

#endif // QQGROUPSTRATEGY_HEJR_H


