#include "GroupContainer_Hejr.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <limits>
using namespace std;

// ---------- 辅助函数：逗号分隔字符串拆分为 vector ----------
static vector<string> splitComma2(const string& s)
{
    vector<string> result;
    stringstream ss(s);
    string token;
    while (getline(ss, token, ','))
    {
        if (!token.empty())
            result.push_back(token);
    }
    return result;
}

// ---------- 辅助函数：vector<string> 合并为逗号分隔字符串 ----------
static string joinComma2(const vector<string>& v)
{
    string result;
    for (size_t i = 0; i < v.size(); i++)
    {
        if (i > 0) result += ",";
        result += v[i];
    }
    return result;
}

// ---------- 构造 ----------
GroupContainer_Hejr::GroupContainer_Hejr(string fname)
{
    filename    = fname;
    nextQQId = nextWeChatId = 1007;
    qqStrategy  = new QQGroupStrategy_Hejr();
    wxStrategy   = new WeChatGroupStrategy_Hejr();

    // 尝试从文件加载；若文件不存在则创建默认群
    ifstream fin(fname.c_str());
    if (fin.is_open())
    {
        fin.close();
        load(fname);
    }
    else
    {
        cout << "[群容器] 文件 " << fname << " 不存在，初始化 QQ、微信预设群。" << endl;
        initDefaultGroups();
        save(fname);
    }

}

// ---------- 析构 ----------
GroupContainer_Hejr::~GroupContainer_Hejr()
{
    // 释放群对象内存（策略对象由容器统一管理）
    for (size_t i = 0; i < groups.size(); i++)
        delete groups[i];
    groups.clear();

    delete qqStrategy;
    delete wxStrategy;
}

// ---------- 根据类型获取策略指针 ----------
GroupStrategy_Hejr* GroupContainer_Hejr::getStrategy(string type)
{
    if (type == "WeChat" || type == "微信群模式")
        return wxStrategy;
    return qqStrategy;  // 默认QQ策略
}

// ---------- 创建旧版预设群并补齐两个服务的群号 ----------
void GroupContainer_Hejr::initDefaultGroups()
{
    // 保留旧版群名与群主；旧版按当前管理模式归入 QQ 或微信。
    struct GroupInit { string id; string name; string owner; string strat; };
    GroupInit inits[] = {
        {"1001", "QQ交流群",   "10001", "QQ"},
        {"1002", "微信交流群",  "10003", "WeChat"},
        {"1003", "微博广场",    "10002", "QQ"},
        {"1004", "微商商圈",    "10001", "WeChat"},
        {"1005", "微唱天地",    "10002", "QQ"},
        {"1006", "微走联盟",    "10003", "WeChat"}
    };

    for (int i = 0; i < 6; i++)
    {
        GroupStrategy_Hejr* strat = getStrategy(inits[i].strat);
        Group_Hejr* group = new Group_Hejr(inits[i].strat, inits[i].id,
                                          inits[i].name, inits[i].owner, strat);
        group->setLegacyGroupId(inits[i].id);
        groups.push_back(group);
    }
    ensureDefaultGroups();
    cout << "[群容器] 已创建 QQ、微信各 6 个默认群。" << endl;
}

bool GroupContainer_Hejr::ensureDefaultGroups()
{
    bool added = false;
    const string services[] = {"QQ", "WeChat"};
    for (int service = 0; service < 2; ++service)
    {
        for (int number = 1001; number <= 1006; ++number)
        {
            string id = to_string(number);
            if (findByGroupId(services[service] + ":" + id) != NULL) continue;
            string name = (service == 0 ? "QQ群" : "微信群") + id;
            string owner = service == 0 ? "10001" : "10003";
            groups.push_back(new Group_Hejr(services[service], id, name,
                                           owner, getStrategy(services[service])));
            added = true;
        }
    }
    return added;
}

// ---------- 添加群 ----------
void GroupContainer_Hejr::addGroup(Group_Hejr* g)
{
    groups.push_back(g);
    cout << "[群容器] 已添加群: " << g->getStorageId() << endl;
}

// ---------- 按群号查找 ----------
Group_Hejr* GroupContainer_Hejr::createGroup(string service, string name,
                                           string owner,
                                           const vector<string>& selectedMembers)
{
    if (service != "QQ" && service != "WeChat") return NULL;
    if (name.find_first_not_of(" \t\r\n") == string::npos ||
        name.find_first_of("|\r\n") != string::npos)
    {
        cout << "群名不能为空或包含 |、换行符。" << endl;
        return NULL;
    }
    vector<string> initialMembers;
    for (size_t i = 0; i < selectedMembers.size(); ++i)
    {
        const string& id = selectedMembers[i];
        if (!id.empty() && id != owner &&
            find(initialMembers.begin(), initialMembers.end(), id) == initialMembers.end())
            initialMembers.push_back(id);
    }
    if (initialMembers.empty())
    {
        cout << "建群至少要选择一名好友，不能单人建群。" << endl;
        return NULL;
    }
    int& nextId = service == "QQ" ? nextQQId : nextWeChatId;
    while (nextId < numeric_limits<int>::max() &&
           findByGroupId(service + ":" + to_string(nextId)) != NULL) ++nextId;
    if (nextId == numeric_limits<int>::max())
    {
        cout << "该服务群号已用尽。" << endl;
        return NULL;
    }
    Group_Hejr* group = new Group_Hejr(service, to_string(nextId++), name,
                                      owner, getStrategy(service));
    for (size_t i = 0; i < initialMembers.size(); ++i) group->loadMember(initialMembers[i]);
    addGroup(group);
    return group;
}

bool GroupContainer_Hejr::removeGroup(string storageId)
{
    for (vector<Group_Hejr*>::iterator it = groups.begin(); it != groups.end(); ++it)
    {
        if ((*it)->getStorageId() != storageId) continue;
        delete *it;
        groups.erase(it);
        return true;
    }
    return false;
}

// ---------- 按群号查找 ----------
Group_Hejr* GroupContainer_Hejr::findByGroupId(string storageId)
{
    for (size_t i = 0; i < groups.size(); i++)
    {
        if (groups[i]->getStorageId() == storageId)
            return groups[i];
    }
    return NULL;
}

// ---------- 获取所有群 ----------
vector<Group_Hejr*> GroupContainer_Hejr::getAllGroups()
{
    return groups;
}

// ---------- 列出所有群 ----------
void GroupContainer_Hejr::listAll() const
{
    cout << "========== 群列表（共 " << groups.size() << " 个）==========" << endl;
    for (size_t i = 0; i < groups.size(); i++)
    {
        cout << "  服务: " << groups[i]->getServiceType()
             << "  群号: " << groups[i]->getGroupId()
             << "  群名: " << groups[i]->getGroupName()
             << "  群主: " << groups[i]->getOwnerId()
             << "  模式: " << groups[i]->getStrategyName()
             << "  成员: " << groups[i]->getMembers().size() << " 人"
             << endl;
    }
    cout << "=================================" << endl;
}

// ---------- 从文件加载 ----------
void GroupContainer_Hejr::load(string fname)
{
    ifstream fin(fname.c_str());
    if (!fin.is_open())
    {
        cout << "[群容器] 无法打开 " << fname << "。" << endl;
        return;
    }

    // 先释放已有的群对象
    for (size_t i = 0; i < groups.size(); i++)
        delete groups[i];
    groups.clear();

    bool migrated = false;
    bool hasStateHeader = false;
    nextQQId = nextWeChatId = 1007;
    string line;
    while (getline(fin, line))
    {
        if (line.empty()) continue;

        // 新格式第一列为 QQ:1001 等；旧格式只有群号，按原策略归属服务。
        stringstream ss(line);
        string field;
        vector<string> fields;
        while (getline(ss, field, '|'))
            fields.push_back(field);

        if (fields.size() == 3 && fields[0] == "#group-state-v1")
        {
            hasStateHeader = true;
            int nextId;
            if (istringstream(fields[1]) >> nextId) nextQQId = max(nextQQId, nextId);
            if (istringstream(fields[2]) >> nextId) nextWeChatId = max(nextWeChatId, nextId);
            continue;
        }
        if (fields.size() < 4) continue;

        string gid    = fields[0];
        string gname   = fields[1];
        string ownerId = fields[2];
        string stratType = fields[3];

        string serviceType;
        size_t separator = gid.find(':');
        if (separator == string::npos)
        {
            serviceType = stratType == "WeChat" ? "WeChat" : "QQ";
            migrated = true;
        }
        else
        {
            serviceType = gid.substr(0, separator);
            gid = gid.substr(separator + 1);
            if (serviceType != "QQ" && serviceType != "WeChat") continue;
        }
        GroupStrategy_Hejr* strat = getStrategy(stratType);
        int number;
        if (istringstream(gid) >> number && number >= 1007)
        {
            int& nextId = serviceType == "QQ" ? nextQQId : nextWeChatId;
            nextId = max(nextId, number == numeric_limits<int>::max() ? number : number + 1);
        }

        // 用构造函数创建群（群主自动入群）
        Group_Hejr* g = new Group_Hejr(serviceType, gid, gname, ownerId, strat);
        if (separator == string::npos) g->setLegacyGroupId(gid);
        else if (fields.size() > 7) g->setLegacyGroupId(fields[7]);

        // 兼容早期测试数据：把不可操作的 system 群主迁移为测试用户。
        if (ownerId == "system")
        {
            if (gid == "1001" || gid == "1004") g->setOwnerId("10001");
            else if (gid == "1002" || gid == "1006") g->setOwnerId("10003");
            else if (gid == "1003" || gid == "1005") g->setOwnerId("10002");
        }

        // 按保存的加入顺序恢复，不能将现任群主强行排到第一位。
        if (fields.size() > 4)
        {
            g->loadMembers(splitComma2(fields[4]));
        }

        // 加载管理员
        if (fields.size() > 5)
        {
            vector<string> adms = splitComma2(fields[5]);
            for (size_t i = 0; i < adms.size(); i++)
                g->loadAdmin(adms[i]);
        }

        // 加载子群
        if (fields.size() > 6 && !fields[6].empty())
        {
            // 子群以分号分隔，每个子群: name:member1,member2,...
            stringstream sgss(fields[6]);
            string sgToken;
            while (getline(sgss, sgToken, ';'))
            {
                if (sgToken.empty()) continue;

                // 按第一个 : 拆分子群名称和成员
                size_t colonPos = sgToken.find(':');
                if (colonPos != string::npos)
                {
                    string sgName = sgToken.substr(0, colonPos);
                    string sgMems = sgToken.substr(colonPos + 1);
                    vector<string> sgMemList = splitComma2(sgMems);
                    g->loadSubGroup(sgName, sgMemList);
                }
            }
        }

        groups.push_back(g);
    }
    fin.close();

    // 只在旧版文件第一次迁移时补齐预设群；已解散群不会重新生成。
    if (!hasStateHeader)
    {
        if (groups.empty()) initDefaultGroups();
        else ensureDefaultGroups();
        migrated = true;
    }
    if (migrated) save(fname);

    cout << "[群容器] 已从 " << fname << " 加载 "
         << groups.size() << " 个群。" << endl;
}

// ---------- 保存到文件 ----------
void GroupContainer_Hejr::save(string fname) const
{
    ofstream fout(fname.c_str());
    if (!fout.is_open())
    {
        cerr << "[群容器] 无法写入文件 " << fname << "！" << endl;
        return;
    }

    fout << "#group-state-v1|" << nextQQId << "|" << nextWeChatId << endl;
    for (size_t i = 0; i < groups.size(); i++)
    {
        Group_Hejr* g = groups[i];

        // 判断策略类型
        string stratType = (g->getStrategyName().find("微信") != string::npos)
                           ? "WeChat" : "QQ";

        fout << g->getStorageId()   << "|"
             << g->getGroupName()   << "|"
             << g->getOwnerId()     << "|"
             << stratType            << "|"
             << joinComma2(g->getMembers()) << "|"
             << joinComma2(g->getAdmins())  << "|";

        // 子群以分号分隔
        vector<SubGroup_Hejr> sgs = g->getSubGroups();
        for (size_t j = 0; j < sgs.size(); j++)
        {
            if (j > 0) fout << ";";
            fout << sgs[j].name << ":" << joinComma2(sgs[j].members);
        }
        fout << "|" << g->getLegacyGroupId() << endl;
    }
    fout.close();

    cout << "[群容器] 已保存 " << groups.size() << " 个群到 "
         << fname << "。" << endl;
}

