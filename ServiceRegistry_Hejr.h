#ifndef SERVICEREGISTRY_HEJR_H
#define SERVICEREGISTRY_HEJR_H

#include <string>
#include <vector>

struct ServiceDefinition_Hejr
{
    std::string code;
    std::string name;
    bool independentId;
};

// 服务定义统一来源；启动时加载配置，所有服务菜单共用同一顺序。
class ServiceRegistry_Hejr
{
private:
    std::vector<ServiceDefinition_Hejr> definitions;
    ServiceRegistry_Hejr();

public:
    static const ServiceRegistry_Hejr& instance();
    const std::vector<ServiceDefinition_Hejr>& all() const;
    const ServiceDefinition_Hejr* find(const std::string& code) const;
    bool isIndependent(const std::string& code) const;
    std::string codeAt(int menuNumber) const;
    std::string serviceNames() const;
    void printChoices(bool allowCancel = false) const;
};

#endif
