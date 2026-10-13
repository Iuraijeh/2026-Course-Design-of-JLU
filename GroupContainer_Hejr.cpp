#include "GroupContainer_Hejr.h"
#include <iostream>
#include <fstream>
#include <sstream>
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
        cout << "[群容器] 文件 " << fname << " 不存在，初始化默认群 1001~1006。" << endl;
        initDefaultGroups();
        save(fname);
    }

    // 若加载后仍无群，则初始化默认群
    if (groups.empty())
    {
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

// ---------- 创建初始的 1001~1006 六个群 ----------
void GroupContainer_Hejr::initDefaultGroups()
{
    // 群号、群名、群主、初始策略
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
        groups.push_back(new Group_Hejr(inits[i].id, inits[i].name,
                                      inits[i].owner, strat));
    }
    cout << "[群容器] 已创建 6 个默认群（1001~1006）。" << endl;
}

// ---------- 添加群 ----------
void GroupContainer_Hejr::addGroup(Group_Hejr* g)
{
    groups.push_back(g);
    cout << "[群容器] 已添加群: " << g->getGroupId() << endl;
}

// ---------- 按群号查找 ----------
Group_Hejr* GroupContainer_Hejr::findByGroupId(string gid)
{
    for (size_t i = 0; i < groups.size(); i++)
    {
        if (groups[i]->getGroupId() == gid)
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
        cout << "  群号: " << groups[i]->getGroupId()
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

    string line;
    while (getline(fin, line))
    {
        if (line.empty()) continue;

        // 格式: groupId|groupName|ownerId|strategyType|members|admins|subgroups
        stringstream ss(line);
        string field;
        vector<string> fields;
        while (getline(ss, field, '|'))
            fields.push_back(field);

        if (fields.size() < 4) continue;

        string gid    = fields[0];
        string gname   = fields[1];
        string ownerId = fields[2];
        string stratType = fields[3];

        GroupStrategy_Hejr* strat = getStrategy(stratType);

        // 用构造函数创建群（群主自动入群）
        Group_Hejr* g = new Group_Hejr(gid, gname, ownerId, strat);

        // 兼容早期测试数据：把不可操作的 system 群主迁移为测试用户。
        if (ownerId == "system")
        {
            if (gid == "1001" || gid == "1004") g->setOwnerId("10001");
            else if (gid == "1002" || gid == "1006") g->setOwnerId("10003");
            else if (gid == "1003" || gid == "1005") g->setOwnerId("10002");
        }

        // 加载成员（群主已在构造时添加，这里跳过重复）
        if (fields.size() > 4)
        {
            vector<string> mems = splitComma2(fields[4]);
            for (size_t i = 0; i < mems.size(); i++)
            {
                // 旧版本写入的 system 是占位群主，不再视为真实成员。
                if (mems[i] != "system") g->loadMember(mems[i]);
            }
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

    for (size_t i = 0; i < groups.size(); i++)
    {
        Group_Hejr* g = groups[i];

        // 判断策略类型
        string stratType = (g->getStrategyName().find("微信") != string::npos)
                           ? "WeChat" : "QQ";

        fout << g->getGroupId()     << "|"
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
        fout << endl;
    }
    fout.close();

    cout << "[群容器] 已保存 " << groups.size() << " 个群到 "
         << fname << "。" << endl;
}

