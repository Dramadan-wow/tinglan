#pragma once
#include "GroupHsx.h"
namespace im {
class WeChatGroupHsx final : public GroupHsx {
public:
    WeChatGroupHsx(int id, const std::string& name, int owner);
    std::string typeName() const override;
};
}
