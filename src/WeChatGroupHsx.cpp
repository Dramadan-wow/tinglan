#include "WeChatGroupHsx.h"
namespace im {
WeChatGroupHsx::WeChatGroupHsx(int id, const std::string& name, int owner)
    : GroupHsx(id, name, owner, GroupModeHsx::WeChat) {}
std::string WeChatGroupHsx::typeName() const { return "微信群"; }
}
