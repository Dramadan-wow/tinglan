#pragma once
#include <string>
namespace im {
class ConsoleHsx;
class GroupScreenHsx {
public:
    explicit GroupScreenHsx(ConsoleHsx& ui) : ui_(ui) {}
    void show();
private:
    void list(bool mine, const std::string& keyword = "");
    void details(int id);
    void selected(int id);
    void manage(int id);
    ConsoleHsx& ui_;
};
}
