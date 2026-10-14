#pragma once
#include <string>
namespace im {
class NetChatHsx {
public:
    static void chat(bool server, const std::string& ip, int port, const std::string& name);
};
}
