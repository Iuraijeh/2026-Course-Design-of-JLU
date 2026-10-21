#include "MessageStore_Hejr.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <atomic>
#include <thread>
#include <functional>
#include <cctype>

using namespace std;

static const char* MESSAGE_FILE = "messages.txt";
static atomic<unsigned long> messageCounter(0);

string MessageStore_Hejr::encodeHex(const string& value)
{
    static const char* digits = "0123456789abcdef";
    string result;
    result.reserve(value.size() * 2);
    for (size_t i = 0; i < value.size(); ++i)
    {
        unsigned char c = (unsigned char)value[i];
        result += digits[c >> 4];
        result += digits[c & 15];
    }
    return result;
}

string MessageStore_Hejr::decodeHex(const string& value)
{
    if (value.size() % 2 != 0) return "";
    string result;
    for (size_t i = 0; i < value.size(); i += 2)
    {
        unsigned int byte = 0;
        istringstream stream(value.substr(i, 2));
        stream >> hex >> byte;
        if (stream.fail()) return "";
        result += (char)byte;
    }
    return result;
}

MessageRecord_Hejr MessageStore_Hejr::create(const string& kind,
                                             const string& sender,
                                             const string& target,
                                             const string& text,
                                             const string& transport)
{
    MessageRecord_Hejr message;
    message.at = chrono::duration_cast<chrono::milliseconds>(
        chrono::system_clock::now().time_since_epoch()).count();
    ostringstream id;
    id << "cpp-" << message.at << "-" <<
        hash<thread::id>()(this_thread::get_id()) << "-" << ++messageCounter;
    message.id = id.str();
    message.kind = kind;
    message.sender = sender;
    message.target = target;
    message.text = text;
    message.transport = transport;
    return message;
}

vector<MessageRecord_Hejr> MessageStore_Hejr::readAll()
{
    vector<MessageRecord_Hejr> messages;
    ifstream file(MESSAGE_FILE, ios::binary);
    string line;
    while (getline(file, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r') line.resize(line.size() - 1);
        vector<string> fields;
        istringstream input(line);
        string field;
        while (getline(input, field, '|')) fields.push_back(field);
        if (fields.size() != 7) continue;
        MessageRecord_Hejr message;
        try { message.at = stoll(fields[1]); } catch (...) { continue; }
        message.id = fields[0];
        message.kind = fields[2];
        message.sender = fields[3];
        message.target = fields[4];
        message.text = decodeHex(fields[5]);
        message.transport = fields[6];
        messages.push_back(message);
    }
    return messages;
}

bool MessageStore_Hejr::append(const MessageRecord_Hejr& message)
{
    vector<MessageRecord_Hejr> existing = readAll();
    for (size_t i = 0; i < existing.size(); ++i)
        if (existing[i].id == message.id) return false;
    ofstream file(MESSAGE_FILE, ios::binary | ios::app);
    if (!file.is_open()) return false;
    file << message.id << "|" << message.at << "|" << message.kind << "|"
         << message.sender << "|" << message.target << "|"
         << encodeHex(message.text) << "|" << message.transport << "\n";
    file.flush();
    return file.good();
}

vector<MessageRecord_Hejr> MessageStore_Hejr::conversation(const string& account,
                                                              const string& kind,
                                                              const string& target,
                                                              const string& legacyGroupId)
{
    vector<MessageRecord_Hejr> result;
    vector<MessageRecord_Hejr> all = readAll();
    for (size_t i = 0; i < all.size(); ++i)
    {
        const MessageRecord_Hejr& message = all[i];
        if (kind == "group" && message.kind == "group" &&
            (message.target == target ||
             (!legacyGroupId.empty() && message.target == legacyGroupId)))
            result.push_back(message);
        else if (kind == "direct" && message.kind == "direct" &&
                 ((message.sender == account && message.target == target) ||
                  (message.sender == target && message.target == account)))
            result.push_back(message);
    }
    return result;
}

string MessageStore_Hejr::formatTime(long long milliseconds)
{
    time_t seconds = (time_t)(milliseconds / 1000);
    tm localTime;
#ifdef _WIN32
    localtime_s(&localTime, &seconds);
#else
    localtime_r(&seconds, &localTime);
#endif
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return buffer;
}

string MessageStore_Hejr::displayIdForAccount(const string& account)
{
    string normalized = account;
    for (size_t i = 0; i < normalized.size(); ++i)
        normalized[i] = (char)tolower((unsigned char)normalized[i]);
    ifstream file("users.txt", ios::binary);
    string line;
    while (getline(file, line))
    {
        size_t first = line.find('|');
        if (first == string::npos) continue;
        string storedAccount = line.substr(0, first);
        for (size_t i = 0; i < storedAccount.size(); ++i)
            storedAccount[i] = (char)tolower((unsigned char)storedAccount[i]);
        if (storedAccount != normalized) continue;
        size_t second = line.find('|', first + 1);
        string userId = line.substr(first + 1, second == string::npos
            ? string::npos : second - first - 1);
        return userId.empty() ? "未设置昵称" : userId;
    }
    return "未知用户";
}
