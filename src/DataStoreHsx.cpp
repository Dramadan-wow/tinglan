#include "DataStoreHsx.h"
#include <limits>
namespace im {
const UserHsx& DataStoreHsx::user(int id) const {
    const auto found = users_.find(id); require(found != users_.end(), "用户不存在"); return *found->second;
}
const ServiceAccountHsx& DataStoreHsx::account(int id, ServiceHsx type) const {
    const auto& services = user(id).services();
    const auto found = services.find(type); require(found != services.end(), "用户未开通 " + serviceName(type)); return found->second;
}
const GroupHsx& DataStoreHsx::group(int id) const {
    const auto found = groups_.find(id); require(found != groups_.end(), "群不存在"); return *found->second;
}
const MessageHsx& DataStoreHsx::message(long long id) const {
    const auto found = messages_.find(id); require(found != messages_.end(), "消息不存在"); return found->second;
}
void DataStoreHsx::requireOnline(int id) const { require(user(id).loginAllowed(), "此账号已被管理员禁止登录"); require(user(id).online(), "请先登录"); }
UserHsx& DataStoreHsx::editUser(int id) { user(id); return *users_.at(id); }
ServiceAccountHsx& DataStoreHsx::editAccount(int id, ServiceHsx type) { account(id, type); return editUser(id).services_.at(type); }
GroupHsx& DataStoreHsx::editGroup(int id) { group(id); return *groups_.at(id); }
void DataStoreHsx::audit(const std::string& actor, const std::string& action, int target, ServiceHsx service) {
    if (adminHash_.empty()) return;
    require(nextAudit_ < std::numeric_limits<long long>::max(), "日志编号已用尽");
    const auto id = nextAudit_++; adminLogs_.emplace(id, AdminLogHsx(id, now(), actor, action, target, service));
    while (adminLogs_.size() > 2000) adminLogs_.erase(adminLogs_.begin());
    dirty_ = true;
}
}
