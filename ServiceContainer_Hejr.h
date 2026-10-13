#ifndef SERVICECONTAINER_HEJR_H
#define SERVICECONTAINER_HEJR_H

#include <vector>
#include <string>
#include "ServiceBase_Hejr.h"
using namespace std;

// ============================================================
// 已开通服务容器类 ServiceContainer_Hejr
// 职责：管理某用户已开通的微X服务（ServiceBase_Hejr* 指针）
// 说明：
//   - 服务对象由本容器创建并管理（new / delete）；
//   - 容器析构时释放所有服务对象内存；
//   - 题目要求"开通管理"：用户选择开通 N 个微X服务，
//     openService() 负责实例化对应服务并加入容器。
// 类名后缀 _Hejr 为姓名缩写占位符，请替换。
// ============================================================
class ServiceContainer_Hejr
{
private:
    string userId;                          // 所属用户ID
    vector<ServiceBase_Hejr*> services;       // 已开通的服务列表

public:
    // 构造
    ServiceContainer_Hejr(string uid);

    // 析构：释放服务对象内存
    ~ServiceContainer_Hejr();

    // ---------- 开通 / 关闭服务 ----------
    // 开通服务：以统一服务实现创建 QQ / WeChat / Weibo 入口
    bool openService(string serviceType);

    // 关闭服务
    bool closeService(string serviceType);

    // 查询是否已开通某服务
    bool isOpened(string serviceType) const;

    // ---------- 获取服务 ----------
    ServiceBase_Hejr* getService(string serviceType);  // 返回指针，未找到返回 NULL
    vector<ServiceBase_Hejr*> getAllServices() const;

    // 获取已开通服务类型列表
    vector<string> getOpenedServiceTypes() const;

    // 打印已开通服务
    void listAll() const;
};

#endif // SERVICECONTAINER_HEJR_H


