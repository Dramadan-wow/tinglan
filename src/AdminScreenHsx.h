#pragma once
namespace im {
class ConsoleHsx;
class AdminScreenHsx final {
public:
    explicit AdminScreenHsx(ConsoleHsx& ui) : ui_(ui) {}
    void show();
private:
    ConsoleHsx& ui_;
};
}
