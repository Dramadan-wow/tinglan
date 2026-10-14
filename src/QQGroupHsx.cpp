#include "QQGroupHsx.h"
namespace im {
QQGroupHsx::QQGroupHsx(int id, const std::string& name, int owner, int parent)
    : GroupHsx(id, name, owner, GroupModeHsx::QQ, parent) {}
std::string QQGroupHsx::typeName() const { return "QQ群"; }
}
