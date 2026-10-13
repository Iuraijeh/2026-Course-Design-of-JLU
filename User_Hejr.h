#ifndef USER_HEJR_H
#define USER_HEJR_H

#include <string>
#include <vector>
using namespace std;

// ============================================================
// 用户基本信息类 User_Hejr
// 说明：类名后缀 _Hejr 为姓名缩写占位符，请替换为自己的姓名缩写
// ============================================================
class User_Hejr
{
private:
    string accountNumber;         // 不可修改的帐号号
    string id;                    // 历史数据中的第二列，现在作为昵称
    string nickname;              // 显示名称，与第二列保持同步
    string wechatId;              // 微信独立账号号
    string wechatIdChangedMonth; // 最近一次手动修改月份 YYYY-MM
    string wechatBoundQQ;         // 主动绑定的 QQ 账号号，空值表示未绑定
    string birthday;              // 出生时间  格式 YYYY-MM-DD
    string registerTime;          // 注册时间  格式 YYYY-MM-DD（T龄基准）
    string location;              // 所在地
    string password;              // 平台统一登录密码
    vector<string> openedServices; // 已开通的微X服务列表
    vector<string> friendIds;      // 好友ID列表
    vector<string> groupIds;       // 群ID列表

public:
    // 默认构造
    User_Hejr();

    // 带参构造
    User_Hejr(string uid, string nick, string birth,
            string regTime, string loc);

    // ---- 基本信息 getter / setter ----
    string getId() const;
    void   setId(string uid);

    string getAccountNumber() const;
    void   setAccountNumber(string account);

    string getNickname() const;
    void   setNickname(string nick);

    string getWeChatId() const;
    void   setWeChatId(string value);
    string getWeChatIdChangedMonth() const;
    void   setWeChatIdChangedMonth(string month);
    string getWeChatBoundQQ() const;
    void   setWeChatBoundQQ(string account);

    string getBirthday() const;
    void   setBirthday(string birth);

    string getRegisterTime() const;
    void   setRegisterTime(string regTime);

    string getLocation() const;
    void   setLocation(string loc);

    // 密码校验与修改：密码以用户记录形式持久化，不在服务类中硬编码。
    bool   checkPassword(string candidate) const;
    void   changePassword(string newPassword);
    string getPassword() const;

    // 计算 T龄（注册年数）
    int getTAge() const;

    // ---- 开通服务管理 ----
    void   addService(string serviceType);
    void   removeService(string serviceType);
    bool   isServiceOpened(string serviceType) const;
    vector<string> getOpenedServices() const;

    // ---- 好友ID管理 ----
    void   addFriendId(string fid);
    void   removeFriendId(string fid);
    vector<string> getFriendIds() const;

    // ---- 群ID管理 ----
    void   addGroupId(string gid);
    void   removeGroupId(string gid);
    vector<string> getGroupIds() const;

    // 打印用户全部信息
    void printInfo() const;
};

#endif // USER_HEJR_H


