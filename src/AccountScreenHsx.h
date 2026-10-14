#pragma once
namespace im {
class ConsoleHsx;
class AccountScreenHsx {
public:
    explicit AccountScreenHsx(ConsoleHsx& ui) : ui_(ui) {}
    void show();
private:
    void login();
    void registerUser();
    void services();
    ConsoleHsx& ui_;
};
}
