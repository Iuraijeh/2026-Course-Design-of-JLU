#include "QQGroupStrategy_Hejr.h"

// QQ群：允许申请加入
bool QQGroupStrategy_Hejr::canApplyJoin()
{
    return true;
}

// QQ群：允许邀请加入
bool QQGroupStrategy_Hejr::canInviteJoin()
{
    return true;
}

// QQ群：允许创建临时讨论组（子群）
bool QQGroupStrategy_Hejr::canCreateSubgroup()
{
    return true;
}

// QQ群：拥有管理员制度（群主 + 管理员）
bool QQGroupStrategy_Hejr::hasAdminSystem()
{
    return true;
}

// 策略名称
string QQGroupStrategy_Hejr::getStrategyName()
{
    return "QQ群模式";
}

