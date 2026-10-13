#ifndef MESSAGESTORE_HEJR_H
#define MESSAGESTORE_HEJR_H

#include <string>
#include <vector>

struct MessageRecord_Hejr
{
    std::string id;
    long long at;
    std::string kind;
    std::string sender;
    std::string target;
    std::string text;
    std::string transport;
};

class MessageStore_Hejr
{
public:
    static MessageRecord_Hejr create(const std::string& kind,
                                     const std::string& sender,
                                     const std::string& target,
                                     const std::string& text,
                                     const std::string& transport);
    static bool append(const MessageRecord_Hejr& message);
    static std::vector<MessageRecord_Hejr> readAll();
    static std::vector<MessageRecord_Hejr> conversation(const std::string& account,
                                                         const std::string& kind,
                                                         const std::string& target);
    static std::string encodeHex(const std::string& value);
    static std::string decodeHex(const std::string& value);
    static std::string formatTime(long long milliseconds);
    static std::string displayIdForAccount(const std::string& account);
};

#endif
