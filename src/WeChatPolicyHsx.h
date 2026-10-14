#pragma once
#include "GroupPolicyHsx.h"
namespace im {
class WeChatPolicyHsx final : public GroupPolicyHsx {
public:
    GroupModeHsx mode() const override;
    bool applications() const override;
    bool subgroups() const override;
    bool administrators() const override;
    std::string description() const override;
};
}
