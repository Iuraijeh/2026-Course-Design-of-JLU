#ifndef FRIEND_HEJR_H
#define FRIEND_HEJR_H

#include <string>
#include <vector>
using namespace std;

// ============================================================
// 好友信息类 Friend_Hejr
// 说明：类名后缀 _Hejr 为姓名缩写占位符，请替换为自己的姓名缩写
//       （例如张三改为 Friend_ZhS）
// ============================================================
class Friend_Hejr
{
private:
    string friendId;     // 好友ID
    string remark;       // 备注名
    vector<string> serviceTypes;  // 可同时存在于多个服务中

public:
    // 默认构造
    Friend_Hejr();

    // 带参构造
    Friend_Hejr(string id, string rmk, string svc);

    // ---- 接口（getter / setter） ----
    string getFriendId() const;
    void   setFriendId(string id);

    string getRemark() const;
    void   setRemark(string rmk);

    string getServiceType() const;
    void   setServiceType(string svc);
    vector<string> getServiceTypes() const;
    bool   hasServiceType(string svc) const;
    bool   addServiceType(string svc);
    bool   removeServiceType(string svc);

    // 显示好友信息
    void printInfo() const;
};

#endif // FRIEND_HEJR_H


