#pragma once
#include "UserHsx.h"
namespace im {
class WeChatUserHsx final : public UserHsx {
public:
    using UserHsx::UserHsx;
    std::string typeName() const override;
    bool sharesQQ() const override;
};
}
