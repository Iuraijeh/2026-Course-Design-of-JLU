#ifndef MICROXSERVICE_HEJR_H
#define MICROXSERVICE_HEJR_H

#include "ServiceBase_Hejr.h"

// QQ、微信、微博共用的服务实现。服务类型仅作为资料归属标签，
// 登录、好友和共同好友等能力由这一处统一维护。
class MicroXService_Hejr : public ServiceBase_Hejr
{
public:
    MicroXService_Hejr(string uid, string svcType);

    virtual bool login(string password);
    virtual void addFriend(string friendId);
    virtual void removeFriend(string friendId);
    virtual string getServiceType();
};

#endif // MICROXSERVICE_HEJR_H
