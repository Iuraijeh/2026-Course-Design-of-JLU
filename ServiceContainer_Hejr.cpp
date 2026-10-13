#include "ServiceContainer_Hejr.h"
#include "MicroXService_Hejr.h"
#include <iostream>
#include <algorithm>
using namespace std;

// ---------- 构造 ----------
ServiceContainer_Hejr::ServiceContainer_Hejr(string uid)
{
    userId = uid;
}

// ---------- 析构：释放内存 ----------
ServiceContainer_Hejr::~ServiceContainer_Hejr()
{
    for (size_t i = 0; i < services.size(); i++)
        delete services[i];
    services.clear();
}

// ---------- 开通服务 ----------
bool ServiceContainer_Hejr::openService(string serviceType)
{
    // 检查是否已开通
    if (isOpened(serviceType))
    {
        cout << "[服务容器] " << serviceType << " 服务已开通。" << endl;
        return false;
    }

    ServiceBase_Hejr* svc = NULL;

    if (serviceType != "QQ" && serviceType != "WeChat" && serviceType != "Weibo")
    {
        cout << "[服务容器] 未知服务类型: " << serviceType << endl;
        return false;
    }

    svc = new MicroXService_Hejr(userId, serviceType);

    services.push_back(svc);
    cout << "[服务容器] 用户 " << userId << " 已开通 "
         << serviceType << " 服务。" << endl;
    return true;
}

// ---------- 关闭服务 ----------
bool ServiceContainer_Hejr::closeService(string serviceType)
{
    for (size_t i = 0; i < services.size(); i++)
    {
        if (services[i]->getServiceType() == serviceType)
        {
            delete services[i];
            services.erase(services.begin() + i);
            cout << "[服务容器] 用户 " << userId << " 已关闭 "
                 << serviceType << " 服务。" << endl;
            return true;
        }
    }
    cout << "[服务容器] " << serviceType << " 服务未开通。" << endl;
    return false;
}

// ---------- 查询是否已开通 ----------
bool ServiceContainer_Hejr::isOpened(string serviceType) const
{
    for (size_t i = 0; i < services.size(); i++)
    {
        if (services[i]->getServiceType() == serviceType)
            return true;
    }
    return false;
}

// ---------- 获取指定服务 ----------
ServiceBase_Hejr* ServiceContainer_Hejr::getService(string serviceType)
{
    for (size_t i = 0; i < services.size(); i++)
    {
        if (services[i]->getServiceType() == serviceType)
            return services[i];
    }
    return NULL;
}

// ---------- 获取所有服务 ----------
vector<ServiceBase_Hejr*> ServiceContainer_Hejr::getAllServices() const
{
    return services;
}

// ---------- 获取已开通服务类型列表 ----------
vector<string> ServiceContainer_Hejr::getOpenedServiceTypes() const
{
    vector<string> types;
    for (size_t i = 0; i < services.size(); i++)
        types.push_back(services[i]->getServiceType());
    return types;
}

// ---------- 打印已开通服务 ----------
void ServiceContainer_Hejr::listAll() const
{
    cout << "========== 用户 " << userId << " 已开通服务（共 "
         << services.size() << " 个）==========" << endl;
    for (size_t i = 0; i < services.size(); i++)
    {
        cout << "  服务: " << services[i]->getServiceType()
             << "  服务账号号: " << services[i]->getServiceId()
             << "  登录: " << (services[i]->isLoggedIn() ? "在线" : "离线")
             << endl;
    }
    if (services.empty())
        cout << "  （暂未开通任何服务）" << endl;
    cout << "=================================" << endl;
}

