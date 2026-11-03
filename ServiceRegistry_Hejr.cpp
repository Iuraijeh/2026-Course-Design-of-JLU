#include "ServiceRegistry_Hejr.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace std;

static string trimConfig(const string& value)
{
    size_t first = value.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

static string lowercaseCode(string value)
{
    for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}

static bool validServiceCode(const string& value)
{
    if (value.empty() || value.size() > 32) return false;
    for (size_t i = 0; i < value.size(); ++i)
    {
        char c = value[i];
        bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (!letter && (i == 0 || (c != '_' && (c < '0' || c > '9')))) return false;
    }
    return true;
}

// 答辩定位：服务配置：读取 services.cfg；基础服务编号固定，新增服务指定共享或独立账号。
ServiceRegistry_Hejr::ServiceRegistry_Hejr()
{
    // 保持原来的 1/2/3 菜单编号；基础服务的身份规则不可被配置改变。
    definitions = {{"QQ", "QQ", false}, {"WeChat", "微信", true}, {"Weibo", "微博", false}};
    ifstream file("services.cfg");
    if (!file.is_open()) return;
    string line;
    int number = 0;
    set<string> configuredCodes;
    while (getline(file, line))
    {
        ++number;
        if (number == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        line = trimConfig(line);
        if (line.empty() || line[0] == '#') continue;
        auto fail = [number](const string& message) {
            throw runtime_error("services.cfg 第 " + to_string(number) + " 行：" + message);
        };
        if (count(line.begin(), line.end(), '|') != 2) fail("需要 服务标识|显示名称|账号规则 三列。");
        istringstream input(line);
        string code, name, rule;
        getline(input, code, '|'); getline(input, name, '|'); getline(input, rule);
        code = trimConfig(code); name = trimConfig(name); rule = trimConfig(rule);
        if (!validServiceCode(code)) fail("服务标识须以英文字母开头，仅含字母、数字、下划线，最多32位。");
        if (name.empty() || name.size() > 128 ||
            any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32; }))
            fail("显示名称不能为空或包含控制字符，最多128字节。");
        if (rule != "shared" && rule != "independent") fail("账号规则只能是 shared 或 independent。");
        string key = lowercaseCode(code);
        if (!configuredCodes.insert(key).second) fail("服务标识重复。");
        bool matched = false;
        for (ServiceDefinition_Hejr& definition : definitions)
        {
            if (lowercaseCode(definition.code) != key) continue;
            if (definition.code != code) fail("服务标识大小写与已有定义冲突。");
            if (definition.independentId != (rule == "independent")) fail("不能改变基础服务的账号规则。");
            definition.name = name;
            matched = true;
            break;
        }
        if (!matched) definitions.push_back({code, name, rule == "independent"});
    }
    if (file.bad()) throw runtime_error("无法完整读取 services.cfg，请检查文件后重启。");
}

const ServiceRegistry_Hejr& ServiceRegistry_Hejr::instance()
{
    static const ServiceRegistry_Hejr registry;
    return registry;
}

const vector<ServiceDefinition_Hejr>& ServiceRegistry_Hejr::all() const { return definitions; }

const ServiceDefinition_Hejr* ServiceRegistry_Hejr::find(const string& code) const
{
    for (const ServiceDefinition_Hejr& definition : definitions)
        if (definition.code == code) return &definition;
    return NULL;
}

bool ServiceRegistry_Hejr::isIndependent(const string& code) const
{
    const ServiceDefinition_Hejr* definition = find(code);
    return definition != NULL && definition->independentId;
}

// 答辩定位：服务菜单映射：将显示的数字选项映射为配置中的服务标识。
string ServiceRegistry_Hejr::codeAt(int menuNumber) const
{
    if (menuNumber < 1 || static_cast<size_t>(menuNumber) > definitions.size()) return "";
    return definitions[menuNumber - 1].code;
}

string ServiceRegistry_Hejr::serviceNames() const
{
    string names;
    for (const ServiceDefinition_Hejr& definition : definitions)
    {
        if (!names.empty()) names += "/";
        names += definition.code;
    }
    return names;
}

void ServiceRegistry_Hejr::printChoices(bool allowCancel) const
{
    for (size_t i = 0; i < definitions.size(); ++i)
        cout << "  " << i + 1 << "." << definitions[i].code << "（" << definitions[i].name << "）";
    if (allowCancel) cout << "  0.取消";
    cout << endl;
}
