#ifndef GROUPSTRATEGY_HEJR_H
#define GROUPSTRATEGY_HEJR_H

#include <string>
using namespace std;

// ============================================================
// 群管理策略抽象接口 GroupStrategy_Hejr（策略模式）
// 说明：
//   - 不同微X的群"管理理念"不同，将这些差异点抽象为策略接口；
//   - QQ群：可申请加入、可邀请、可建临时讨论组(子群)、有管理员制度；
//   - 微信群：只能邀请加入、不可建子群、仅群主为特权账号；
//   - Group_Hejr 通过组合持有本接口指针，运行时可调用 setStrategy
//     动态切换管理模式，而群成员数据保持不变。
//   - 类名后缀 _Hejr 为姓名缩写占位符，请替换。
// ============================================================
class GroupStrategy_Hejr
{
public:
    // 虚析构：保证通过基类指针释放派生类对象正确
    virtual ~GroupStrategy_Hejr() {}

    // 是否允许"申请加入"（QQ群 true，微信群 false）
    virtual bool canApplyJoin() = 0;

    // 是否允许"推荐/邀请加入"（QQ群、微信群均允许）
    virtual bool canInviteJoin() = 0;

    // 是否允许创建临时讨论组（子群）（QQ群 true，微信群 false）
    virtual bool canCreateSubgroup() = 0;

    // 是否拥有群主之外的管理员制度（QQ群 true，微信群 false）
    virtual bool hasAdminSystem() = 0;

    // 获取策略名称（如 "QQ群模式" / "微信群模式"）
    virtual string getStrategyName() = 0;
};

#endif // GROUPSTRATEGY_HEJR_H


