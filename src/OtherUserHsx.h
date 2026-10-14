#pragma once
#include "UserHsx.h"
namespace im {
class OtherUserHsx final : public UserHsx {
public:
    OtherUserHsx(int id, const std::string& name, DateHsx birth, DateHsx applied, const std::string& location, bool shared);
    std::string typeName() const override;
    bool sharesQQ() const override;
private:
    bool shared_;
};
}
