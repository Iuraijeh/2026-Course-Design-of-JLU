// ============================================================
// TcpChat_Hejr.cpp — TCP 点对点通信实现（选做模块）
// 依赖：winsock2.h, ws2_32.lib
// 说明：
//   - 服务端：socket→bind→listen→accept→recv
//   - 客户端：socket→connect→send
//   - 接收循环在独立线程中运行，收到消息后打印到控制台
// ============================================================

// 注意：winsock2.h 必须在 windows.h 之前包含，否则会有重定义冲突
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
#endif

#include "TcpChat_Hejr.h"
#include "MessageStore_Hejr.h"
#include <iostream>
#include <thread>
#include <cstring>
#include <sstream>
using namespace std;

// ============================================================
// 构造
// ============================================================
TcpChat_Hejr::TcpChat_Hejr()
{
    wsaStarted    = false;
    serverPort    = 0;
    listening     = false;
    listenThread  = NULL;
}

// ============================================================
// 析构：停止监听，清理 Winsock
// ============================================================
TcpChat_Hejr::~TcpChat_Hejr()
{
    stopServer();
    cleanupWinsock();
}

// ============================================================
// 初始化 Winsock（WSAStartup）
// ============================================================
bool TcpChat_Hejr::initWinsock()
{
    if (wsaStarted) return true;

    // 请求 Winsock 2.2 版本
    WORD ver = MAKEWORD(2, 2);
    WSADATA wsaData;

    int err = WSAStartup(ver, &wsaData);
    if (err != 0)
    {
        cerr << "[TCP] WSAStartup 失败，错误码: " << err << endl;
        return false;
    }

    // 检查版本是否符合要求
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        cerr << "[TCP] 不支持 Winsock 2.2 版本。" << endl;
        WSACleanup();
        return false;
    }

    wsaStarted = true;
    cout << "[TCP] Winsock 2.2 初始化成功。" << endl;
    return true;
}

// ============================================================
// 清理 Winsock（WSACleanup）
// ============================================================
void TcpChat_Hejr::cleanupWinsock()
{
    if (wsaStarted)
    {
        WSACleanup();
        wsaStarted = false;
    }
}

// ============================================================
// 服务端：在指定端口启动监听，循环接收消息（独立线程）
// 流程：socket → bind → listen → accept → recv
// ============================================================

// 监听线程函数（线程入口，friend 可访问私有成员）
void serverListenThread(TcpChat_Hejr* self, int port)
{
    // 创建 TCP 套接字（SOCK_STREAM）
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
        cerr << "[TCP] socket() 失败，错误码: " << WSAGetLastError() << endl;
        self->listening = false;
        return;
    }

    // 程序重启时允许立即复用固定监听端口，保证 TCP 接收器常开。
    BOOL reuseAddress = TRUE;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&reuseAddress, sizeof(reuseAddress));

    // 绑定地址和端口
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // 监听所有网卡
    addr.sin_port        = htons(port);           // 端口

    int ret = bind(listenSock, (sockaddr*)&addr, sizeof(addr));
    if (ret == SOCKET_ERROR)
    {
        cerr << "[TCP] bind() 失败，错误码: " << WSAGetLastError() << endl;
        closesocket(listenSock);
        self->listening = false;
        return;
    }

    // 开始监听，等待连接（队列最大 5）
    ret = listen(listenSock, 5);
    if (ret == SOCKET_ERROR)
    {
        cerr << "[TCP] listen() 失败，错误码: " << WSAGetLastError() << endl;
        closesocket(listenSock);
        self->listening = false;
        return;
    }

    cout << "[TCP] 服务端正在端口 " << port << " 监听，等待连接..." << endl;

    // 接受连接并循环接收消息
    while (self->isListening())
    {
        // accept：阻塞等待客户端连接
        sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock, (sockaddr*)&clientAddr, &clientLen);
        if (clientSock == INVALID_SOCKET)
        {
            // 监听已停止时 accept 会返回错误，这是正常的
            break;
        }

        // 获取客户端 IP
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
        cout << "[TCP] 客户端 " << clientIP << " 已连接。" << endl;

        // 将 TCP 字节流接完整，再解析带账号信息的消息协议。
        char buf[1024];
        int recvLen;
        string payload;
        while ((recvLen = recv(clientSock, buf, sizeof(buf), 0)) > 0)
        {
            payload.append(buf, recvLen);
            if (payload.size() > 20000) break;
        }
        if (payload.compare(0, 4, "MX1|") == 0)
        {
            istringstream input(payload);
            string part;
            vector<string> fields;
            while (getline(input, part, '|')) fields.push_back(part);
            if (fields.size() == 6)
            {
                MessageRecord_Hejr message;
                try { message.at = stoll(fields[2]); } catch (...) { message.at = 0; }
                message.id = fields[1];
                message.kind = "direct";
                message.sender = fields[3];
                message.target = fields[4];
                while (!fields[5].empty() && (fields[5].back() == '\n' || fields[5].back() == '\r'))
                    fields[5].pop_back();
                message.text = MessageStore_Hejr::decodeHex(fields[5]);
                message.transport = "tcp";
                if (message.at > 0 && !message.sender.empty() && !message.target.empty())
                {
                    MessageStore_Hejr::append(message);
                    cout << "[TCP] " << MessageStore_Hejr::displayIdForAccount(message.sender)
                         << " -> " << MessageStore_Hejr::displayIdForAccount(message.target)
                         << " [" << MessageStore_Hejr::formatTime(message.at) << "]: "
                         << message.text << endl;
                }
            }
        }
        else if (!payload.empty())
            cout << "[TCP] 收到来自 " << clientIP << " 的旧格式消息: " << payload << endl;

        // recv 返回 0 表示对方关闭连接，返回 < 0 表示出错
        closesocket(clientSock);
        cout << "[TCP] 客户端 " << clientIP << " 已断开。" << endl;
    }

    closesocket(listenSock);
    cout << "[TCP] 服务端已停止监听。" << endl;
}

bool TcpChat_Hejr::startServer(int port)
{
    // 确保 Winsock 已初始化
    if (!initWinsock())
        return false;

    if (listening)
    {
        cout << "[TCP] 已在监听中，请先停止。" << endl;
        return false;
    }

    serverPort = port;
    listening  = true;

    // 在独立线程中运行监听循环，避免阻塞主线程
    thread t(serverListenThread, this, port);
    t.detach();   // 分离线程，后台运行

    return true;
}

// ============================================================
// 停止监听
// ============================================================
void TcpChat_Hejr::stopServer()
{
    if (!listening) return;

    listening = false;

    // 向本机发送一个连接请求，让 accept() 返回以退出循环
    // （简单做法：连接自身端口使 accept 返回并立即关闭）
    if (serverPort > 0)
    {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s != INVALID_SOCKET)
        {
            sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            addr.sin_port        = htons(serverPort);
            connect(s, (sockaddr*)&addr, sizeof(addr));
            closesocket(s);
        }
    }

    cout << "[TCP] 正在停止监听..." << endl;
}

bool TcpChat_Hejr::isListening() const
{
    return listening;
}

// ============================================================
// 客户端：向指定 IP:Port 发送消息
// 流程：socket → connect → send
// ============================================================
bool TcpChat_Hejr::sendMessage(string ip, int port, string msg,
                               string sender, string target)
{
    // 确保 Winsock 已初始化
    if (!initWinsock())
        return false;

    // 创建 TCP 套接字
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        cerr << "[TCP] socket() 失败，错误码: " << WSAGetLastError() << endl;
        return false;
    }

    // 构造服务器地址
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
    serverAddr.sin_port         = htons(port);

    // 连接到服务器
    int ret = connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (ret == SOCKET_ERROR)
    {
        cerr << "[TCP] connect() 失败，无法连接到 " << ip
             << ":" << port << "，错误码: " << WSAGetLastError() << endl;
        closesocket(sock);
        return false;
    }

    cout << "[TCP] 已连接到 " << ip << ":" << port << endl;

    MessageRecord_Hejr record = MessageStore_Hejr::create(
        "direct", sender, target, msg, "tcp");
    string payload = msg;
    if (!sender.empty() && !target.empty())
        payload = "MX1|" + record.id + "|" + to_string(record.at) + "|" +
                  sender + "|" + target + "|" + MessageStore_Hejr::encodeHex(msg) + "\n";

    int sent = 0;
    while (sent < (int)payload.length())
    {
        int count = send(sock, payload.c_str() + sent,
                         (int)payload.length() - sent, 0);
        if (count == SOCKET_ERROR || count == 0) break;
        sent += count;
    }
    if (sent != (int)payload.length())
    {
        cerr << "[TCP] send() 失败，错误码: " << WSAGetLastError() << endl;
        closesocket(sock);
        return false;
    }

    if (!sender.empty() && !target.empty())
        MessageStore_Hejr::append(record);
    cout << "[TCP] 已发送 " << sent << " 字节: " << msg << endl;

    // 关闭套接字
    closesocket(sock);
    return true;
}

// ============================================================
// 获取本机 IP 地址
// ============================================================
string TcpChat_Hejr::getLocalIP()
{
    if (!initWinsock()) return "127.0.0.1";

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR)
    {
        return "127.0.0.1";
    }

    // 通过主机名获取地址信息
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = NULL;
    if (getaddrinfo(hostname, NULL, &hints, &result) != 0)
    {
        return "127.0.0.1";
    }

    char ipStr[INET_ADDRSTRLEN] = "127.0.0.1";
    for (struct addrinfo* p = result; p != NULL; p = p->ai_next)
    {
        sockaddr_in* sa = (sockaddr_in*)p->ai_addr;
        inet_ntop(AF_INET, &sa->sin_addr, ipStr, sizeof(ipStr));
        break;  // 取第一个
    }

    freeaddrinfo(result);
    return string(ipStr);
}

