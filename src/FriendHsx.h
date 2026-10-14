#pragma once
#include "TypesHsx.h"

namespace im {
class FriendHsx {
public:
    FriendHsx(int user, ServiceHsx contact, std::string remark, std::string category);
    int user() const { return user_; }
    ServiceHsx contact() const { return contact_; }
    const std::string& remark() const { return remark_; }
    const std::string& category() const { return category_; }
private:
    friend class FriendManagerHsx;
    int user_;
    ServiceHsx contact_;
    std::string remark_, category_;
};
}
