#ifndef TCPCHAT_HEJR_H
#define TCPCHAT_HEJR_H

#include <string>
using namespace std;

// ============================================================
// TCP 点对点通信类 TcpChat_Hejr（选做模块）
// 说明：
//   - 基于 Windows Sockets 2 (winsock2.h) 实现点对点 TCP 通信
//   - 链接静态库 ws2_32.lib（通过 #pragma comment 实现）
//   - 服务端：socket→bind→listen→accept→recv（收消息）
//   - 客户端：socket→connect→send（发消息）
//   - 服务端接收循环放在独立线程中，避免阻塞主线程
//   - 类名后缀 _Hejr 为姓名缩写占位符，请替换
// ============================================================

// 链接 ws2_32.lib 静态库（MSVC / MinGW 均支持）
#pragma comment(lib, "ws2_32.lib")

// 前向声明：服务端接收线程函数
class TcpChat_Hejr;

class TcpChat_Hejr
{
private:
    friend void serverListenThread(TcpChat_Hejr* self, int port);

    bool   wsaStarted;      // WSAStartup 是否已初始化
    int    serverPort;       // 本机监听端口（服务端模式）
    bool   listening;        // 是否正在监听
    void*  listenThread;     // 监听线程句柄（HANDLE）

    // 内部：初始化 Winsock
    bool initWinsock();

    // 内部：清理 Winsock
    void cleanupWinsock();

public:
    // 构造
    TcpChat_Hejr();

    // 析构：停止监听并清理 Winsock
    ~TcpChat_Hejr();

    // ---------- 服务端模式 ----------
    // 启动服务器：在指定端口监听，接受连接并接收消息
    // 在独立线程中循环接收，收到消息后通过回调显示
    bool startServer(int port);

    // 停止监听（仅在平台退出时调用）
    void stopServer();

    // 是否正在监听
    bool isListening() const;

    // ---------- 客户端模式 ----------
    // 向指定 IP:Port 发送一条消息
    bool sendMessage(string ip, int port, string msg,
                     string sender = "", string target = "");

    // ---------- 工具 ----------
    // 获取本机 IP 地址（用于告知对方连接地址）
    string getLocalIP();
};

#endif // TCPCHAT_HEJR_H


