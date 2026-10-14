#pragma once
#include "FriendHsx.h"
#include <map>
#include <set>

namespace im {
class ServiceAccountHsx {
public:
    ServiceAccountHsx(ServiceHsx type, long long number, std::string nick, std::string password, bool shared);
    ServiceHsx type() const { return type_; }
    long long number() const { return number_; }
    const std::string& nick() const { return nick_; }
    bool shared() const { return shared_; }
    long long boundQQ() const { return boundQQ_; }
    bool verify(const std::string& password) const;
    const std::map<int, FriendHsx>& friends() const { return friends_; }
    const std::set<int>& blocked() const { return blocked_; }
private:
    friend class WorkspaceCodecHsx;
    friend class WorkspaceBackendHsx;
    friend class AccountManagerHsx;
    friend class AdminManagerHsx;
    friend class FriendManagerHsx;
    friend class StorageHsx;
    ServiceHsx type_;
    long long number_, boundQQ_ = 0;
    std::string nick_, password_;
    bool shared_;
    std::map<int, FriendHsx> friends_;
    std::set<int> blocked_;
};
}
