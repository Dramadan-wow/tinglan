#include "GroupPolicyHsx.h"
#include "QQPolicyHsx.h"
#include "WeChatPolicyHsx.h"
namespace im {
std::unique_ptr<GroupPolicyHsx> GroupPolicyHsx::create(GroupModeHsx mode) {
    if (mode == GroupModeHsx::QQ) return std::make_unique<QQPolicyHsx>();
    require(mode == GroupModeHsx::WeChat, "群模式无效");
    return std::make_unique<WeChatPolicyHsx>();
}
}
