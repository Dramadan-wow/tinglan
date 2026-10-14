#include "WeChatPolicyHsx.h"
namespace im {
GroupModeHsx WeChatPolicyHsx::mode() const { return GroupModeHsx::WeChat; }
bool WeChatPolicyHsx::applications() const { return false; }
bool WeChatPolicyHsx::subgroups() const { return false; }
bool WeChatPolicyHsx::administrators() const { return false; }
std::string WeChatPolicyHsx::description() const { return "成员邀请入群 / 仅群主拥有管理权限 / 不可新建讨论组"; }
}
