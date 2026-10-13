#include "WeChatGroupStrategy_Hejr.h"

// 微信群：不允许申请加入
bool WeChatGroupStrategy_Hejr::canApplyJoin()
{
    return false;
}

// 微信群：只能推荐/邀请加入
bool WeChatGroupStrategy_Hejr::canInviteJoin()
{
    return true;
}

// 微信群：不允许创建子群（临时讨论组）
bool WeChatGroupStrategy_Hejr::canCreateSubgroup()
{
    return false;
}

// 微信群：没有管理员制度，仅群主有特权
bool WeChatGroupStrategy_Hejr::hasAdminSystem()
{
    return false;
}

// 策略名称
string WeChatGroupStrategy_Hejr::getStrategyName()
{
    return "微信群模式";
}

