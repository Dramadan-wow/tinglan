#pragma once
#include "UserHsx.h"
namespace im {
class QQUserHsx final : public UserHsx {
public:
    using UserHsx::UserHsx;
    std::string typeName() const override;
    bool sharesQQ() const override;
};
}
