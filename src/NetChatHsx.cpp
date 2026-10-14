#include "NetChatHsx.h"
#include "ScopeHsx.h"
#include "TypesHsx.h"
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <mutex>
#include <thread>
#pragma comment(lib, "ws2_32.lib")
namespace im {
namespace {
// TCP没有消息边界；4字节长度头和完整收发循环共同处理分包、粘包。
bool transfer(SOCKET socket, char* bytes, int length, bool sending, const std::atomic<bool>* active = nullptr) {
    int done = 0;
    while (done < length) {
        if (!sending) {
            if (active && !active->load()) return false;
            fd_set ready; FD_ZERO(&ready); FD_SET(socket, &ready); timeval timeout{0, 250000};
            const int state = select(0, &ready, nullptr, nullptr, &timeout);
            if (state == SOCKET_ERROR) return false; if (state == 0) continue;
        }
        const int n = sending ? send(socket, bytes + done, length - done, 0) : recv(socket, bytes + done, length - done, 0);
        if (n <= 0) return false; done += n;
    }
    return true;
}
bool sendFrame(SOCKET socket, const std::string& text) {
    auto size = htonl(static_cast<std::uint32_t>(text.size()));
    return transfer(socket, reinterpret_cast<char*>(&size), sizeof(size), true) &&
        transfer(socket, const_cast<char*>(text.data()), static_cast<int>(text.size()), true);
}
bool receiveFrame(SOCKET socket, std::string& text, const std::atomic<bool>& active) {
    std::uint32_t size{}; if (!transfer(socket, reinterpret_cast<char*>(&size), sizeof(size), false, &active)) return false;
    size = ntohl(size); if (!size || size > 6000) return false; text.resize(size);
    return transfer(socket, text.data(), static_cast<int>(size), false, &active);
}
}
}
#endif
namespace im {
void NetChatHsx::chat(bool server, const std::string& ip, int port, const std::string& name) {
    require(port >= 1 && port <= 65535, "端口应在1到65535之间"); checkText(name, "聊天名称");
#ifdef _WIN32
    WSADATA runtime{}; require(WSAStartup(MAKEWORD(2, 2), &runtime) == 0, "WinSock初始化失败");
    ScopeHsx cleanupRuntime([] { WSACleanup(); });
    const SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); SOCKET peer = INVALID_SOCKET;
    ScopeHsx cleanupSockets([&] { if (peer != INVALID_SOCKET) closesocket(peer); if (socket != INVALID_SOCKET) closesocket(socket); });
    require(socket != INVALID_SOCKET, "创建套接字失败");
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(static_cast<u_short>(port));
    if (server) {
        address.sin_addr.s_addr = INADDR_ANY;
        require(bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != SOCKET_ERROR, "端口绑定失败");
        require(listen(socket, 1) != SOCKET_ERROR, "监听失败"); std::cout << "等待连接，30秒后超时..." << std::endl;
        fd_set ready; FD_ZERO(&ready); FD_SET(socket, &ready); timeval timeout{30, 0};
        require(select(0, &ready, nullptr, nullptr, &timeout) > 0, "监听超时"); peer = accept(socket, nullptr, nullptr);
        require(peer != INVALID_SOCKET, "接受连接失败");
    } else {
        require(inet_pton(AF_INET, ip.c_str(), &address.sin_addr) == 1, "请输入有效IPv4地址");
        u_long nonblocking = 1; require(ioctlsocket(socket, FIONBIO, &nonblocking) == 0, "连接设置失败");
        const int state = connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        if (state == SOCKET_ERROR) {
            require(WSAGetLastError() == WSAEWOULDBLOCK, "连接失败");
            fd_set writable, errors; FD_ZERO(&writable); FD_ZERO(&errors); FD_SET(socket, &writable); FD_SET(socket, &errors);
            timeval timeout{10, 0}; require(select(0, nullptr, &writable, &errors, &timeout) > 0, "连接超时");
            int error{}, size = sizeof(error);
            require(getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) == 0 && !error, "对方拒绝连接");
        }
        nonblocking = 0; require(ioctlsocket(socket, FIONBIO, &nonblocking) == 0, "连接设置失败");
    }
    const SOCKET connected = server ? peer : socket; const DWORD timeout = 10000;
    require(setsockopt(connected, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0, "发送超时设置失败");
    std::atomic<bool> active{true}; std::mutex output;
    std::cout << "已连接，可同时发送和接收；输入 /quit 返回。" << std::endl;
    std::thread reader([&] {
        std::string message;
        while (receiveFrame(connected, message, active)) { std::lock_guard<std::mutex> lock(output); std::cout << "\n[对方] " << message << std::endl; }
        active = false; std::lock_guard<std::mutex> lock(output); std::cout << "\n连接已关闭；若仍停在输入处，请按回车返回。" << std::endl;
    });
    ScopeHsx stopReader([&] { active = false; shutdown(connected, SD_BOTH); reader.join(); });
    std::string message;
    while (active && std::getline(std::cin, message)) {
        if (!active || message == "/quit") break; if (message.empty()) continue;
        if (name.size() + message.size() + 2 > 6000) { std::lock_guard<std::mutex> lock(output); std::cout << "消息过长。" << std::endl; continue; }
        if (!sendFrame(connected, name + ": " + message)) break;
    }
#else
    (void)server; (void)ip; std::cout << "TCP模块支持Windows；本地模拟聊天可跨平台构建。\n";
#endif
}
}
