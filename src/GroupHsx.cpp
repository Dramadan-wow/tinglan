#include "GroupHsx.h"
#include "QQGroupHsx.h"
#include "WeChatGroupHsx.h"
#include <utility>
namespace im {
GroupHsx::GroupHsx(int id, std::string name, int owner, GroupModeHsx native, int parent)
    : id_(id), owner_(owner), parent_(parent), native_(native), name_(std::move(name)), policy_(GroupPolicyHsx::create(native)) {
    require(id > 0 && owner > 0 && parent >= 0 && parent != id, "群编号或群主无效"); checkText(name_, "群名");
}
bool GroupHsx::contains(int user, ServiceHsx service) const {
    const auto it = members_.find(user);
    return it != members_.end() && it->second.count(service) != 0;
}
bool GroupHsx::manages(int user) const {
    return contains(user) && (user == owner_ || (policy_->administrators() && admins_.count(user)));
}
bool GroupHsx::muted(int user) const {
    const auto found = mutedUntil_.find(user);
    return user != owner_ && found != mutedUntil_.end() && found->second > now();
}
std::unique_ptr<GroupHsx> GroupHsx::create(int id, const std::string& name, int owner, GroupModeHsx native, int parent) {
    if (native == GroupModeHsx::QQ) return std::make_unique<QQGroupHsx>(id, name, owner, parent);
    require(native == GroupModeHsx::WeChat && parent == 0, "微信群不能作为讨论组");
    return std::make_unique<WeChatGroupHsx>(id, name, owner);
}
}
