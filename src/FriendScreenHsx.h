#pragma once
#include <string>
namespace im {
class ConsoleHsx;
class FriendScreenHsx {
public:
    explicit FriendScreenHsx(ConsoleHsx& ui) : ui_(ui) {}
    void show();
private:
    void list(const std::string& keyword = "");
    void requests();
    void recommend();
    void sendRequest(int target);
    ConsoleHsx& ui_;
};
}
