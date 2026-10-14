#pragma once
#include "TypesHsx.h"
namespace im {
class ConsoleHsx;
class MessageScreenHsx {
public:
    explicit MessageScreenHsx(ConsoleHsx& ui) : ui_(ui) {}
    void show();
    void chat(MessageKindHsx kind, int target);
private:
    ConsoleHsx& ui_;
};
}
