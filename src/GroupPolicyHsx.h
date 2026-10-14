#pragma once
#include "TypesHsx.h"
#include <memory>
namespace im {
class GroupPolicyHsx {
public:
    virtual ~GroupPolicyHsx() = default;
    virtual GroupModeHsx mode() const = 0;
    virtual bool applications() const = 0;
    virtual bool subgroups() const = 0;
    virtual bool administrators() const = 0;
    virtual std::string description() const = 0;
    static std::unique_ptr<GroupPolicyHsx> create(GroupModeHsx mode);
};
}
