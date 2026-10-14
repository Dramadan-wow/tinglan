#include "GroupManagerHsx.h"
#include <algorithm>
#include <limits>
namespace im {
void GroupManagerHsx::requireMember(int actor, ServiceHsx type, int group) const {
    data_.requireOnline(actor); data_.account(actor, type);
    require(data_.group(group).contains(actor, type), "当前服务未加入此群");
}
void GroupManagerHsx::requireManager(int actor, ServiceHsx type, int group, bool owner) const {
    requireMember(actor, type, group); const auto& g = data_.group(group);
    require(owner ? g.owner() == actor : g.manages(actor), owner ? "此操作仅限群主" : "需要群主或管理员权限");
}
void GroupManagerHsx::checkJoin(int group, int target, ServiceHsx type) const {
    data_.account(target, type); const auto& g = data_.group(group);
    require(!g.contains(target, type), "该服务已经加入此群");
    require(g.contains(target) || g.members().size() < static_cast<size_t>(g.capacity()), "群人数已满");
    if (g.parent()) require(data_.group(g.parent()).contains(target, type), "请先通过相同服务加入父群");
}
int GroupManagerHsx::create(int actor, ServiceHsx type, const std::string& name, GroupModeHsx mode, int parent) {
    data_.requireOnline(actor); data_.account(actor, type); checkText(name, "群名");
    if (parent) {
        requireManager(actor, type, parent);
        require(!data_.group(parent).parent() && data_.group(parent).policy().subgroups() && mode == GroupModeHsx::QQ,
            "当前群不支持创建讨论组");
    }
    require(data_.nextGroup_ < std::numeric_limits<int>::max(), "群号已用尽");
    const int id = data_.nextGroup_;
    auto group = GroupHsx::create(id, name, actor, mode, parent); group->members_[actor].insert(type);
    data_.groups_.emplace(id, std::move(group)); ++data_.nextGroup_; data_.dirty_ = true; return id;
}
void GroupManagerHsx::apply(int actor, ServiceHsx type, int group) {
    data_.requireOnline(actor); checkJoin(group, actor, type); auto& g = data_.editGroup(group);
    require(g.policy().applications(), "微信群仅能由已有成员邀请");
    require(g.pending_.insert({actor, type}).second, "已经提交申请"); data_.dirty_ = true;
}
void GroupManagerHsx::invite(int actor, ServiceHsx type, int group, int target, ServiceHsx destination) {
    requireMember(actor, type, group); checkJoin(group, target, destination); auto& g = data_.editGroup(group);
    g.members_[target].insert(destination); g.pending_.erase({target, destination}); data_.dirty_ = true;
}
void GroupManagerHsx::approve(int actor, ServiceHsx type, int group, int target, ServiceHsx destination, bool accept) {
    requireManager(actor, type, group); auto& g = data_.editGroup(group);
    require(g.policy().applications(), "当前群模式不处理申请");
    require(g.pending_.count({target, destination}) != 0, "入群申请不存在");
    if (accept) { checkJoin(group, target, destination); g.members_[target].insert(destination); }
    g.pending_.erase({target, destination}); data_.dirty_ = true;
}
void GroupManagerHsx::cancelApplication(int actor, ServiceHsx type, int group) {
    data_.requireOnline(actor); data_.account(actor, type);
    require(data_.editGroup(group).pending_.erase({actor, type}) != 0, "没有待处理入群申请"); data_.dirty_ = true;
}
std::vector<int> GroupManagerHsx::family(int group) const {
    data_.group(group); std::vector<int> result{group};
    for (const auto& entry : data_.groups()) if (entry.second->parent() == group) result.push_back(entry.first);
    return result;
}
void GroupManagerHsx::checkLeave(const GroupHsx& group, int user, const std::set<ServiceHsx>& services) const {
    if (group.owner() != user || group.members().size() > 1) return;
    for (auto type : group.members().at(user)) if (!services.count(type)) return;
    require(false, "群 " + std::to_string(group.id()) + " 仅剩群主，请先解散或邀请新成员");
}
void GroupManagerHsx::drop(GroupHsx& g, int user, const std::set<ServiceHsx>& services) {
    for (auto type : services) g.pending_.erase({user, type});
    auto found = g.members_.find(user); if (found == g.members_.end()) return;
    for (auto type : services) found->second.erase(type);
    if (!found->second.empty()) return;
    g.members_.erase(found); g.admins_.erase(user); g.mutedUntil_.erase(user);
    if (g.owner_ == user) {
        g.owner_ = g.admins_.empty() ? g.members_.begin()->first : *g.admins_.begin();
        g.mutedUntil_.erase(g.owner_);
    }
}
void GroupManagerHsx::leave(int actor, ServiceHsx type, int group) {
    requireMember(actor, type, group); const auto related = family(group);
    for (int id : related) checkLeave(data_.group(id), actor, {type});
    for (int id : related) drop(data_.editGroup(id), actor, {type}); data_.dirty_ = true;
}
void GroupManagerHsx::dropService(int actor, ServiceHsx type) {
    data_.requireOnline(actor); data_.account(actor, type);
    for (const auto& entry : data_.groups()) if (entry.second->contains(actor, type)) checkLeave(*entry.second, actor, {type});
    for (auto& entry : data_.groups_) drop(*entry.second, actor, {type}); data_.dirty_ = true;
}
void GroupManagerHsx::kick(int actor, ServiceHsx type, int group, int target) {
    requireManager(actor, type, group); const auto& root = data_.group(group);
    require(root.contains(target) && target != actor && target != root.owner(), "不能移除自己、群主或非成员");
    require(root.owner() == actor || !root.manages(target), "管理员不能移除其他管理员");
    const auto types = root.members().at(target); const auto related = family(group);
    for (int id : related) checkLeave(data_.group(id), target, types);
    for (int id : related) drop(data_.editGroup(id), target, types); data_.dirty_ = true;
}
void GroupManagerHsx::admin(int actor, ServiceHsx type, int group, int target, bool enabled) {
    requireManager(actor, type, group, true); auto& g = data_.editGroup(group);
    require(g.policy().administrators(), "当前模式仅群主拥有管理权限");
    require(g.contains(target) && target != g.owner(), "请选择其他群成员");
    if (enabled) g.admins_.insert(target); else g.admins_.erase(target); data_.dirty_ = true;
}
void GroupManagerHsx::transfer(int actor, ServiceHsx type, int group, int target) {
    requireManager(actor, type, group, true); auto& g = data_.editGroup(group);
    require(g.contains(target) && target != actor, "新群主应是其他群成员");
    g.owner_ = target; g.mutedUntil_.erase(target); data_.dirty_ = true;
}
void GroupManagerHsx::edit(int actor, ServiceHsx type, int group, const std::string& name,
    const std::string& announcement, int capacity) {
    requireManager(actor, type, group); checkText(name, "群名"); checkText(announcement, "公告", true, 3000);
    auto& g = data_.editGroup(group);
    require(capacity >= 1 && capacity <= 5000 && static_cast<size_t>(capacity) >= g.members().size(), "容量应为1到5000且不小于现有人数");
    g.name_ = name; g.announcement_ = announcement; g.capacity_ = capacity; data_.dirty_ = true;
}
void GroupManagerHsx::mode(int actor, ServiceHsx type, int group, GroupModeHsx mode) {
    requireManager(actor, type, group, true);
    data_.editGroup(group).policy_ = GroupPolicyHsx::create(mode); data_.dirty_ = true;
}
void GroupManagerHsx::mute(int actor, ServiceHsx type, int group, int target, int minutes) {
    requireManager(actor, type, group); auto& g = data_.editGroup(group);
    require(g.contains(target) && target != actor && target != g.owner(), "不能禁言自己、群主或非成员");
    require(actor == g.owner() || !g.manages(target), "管理员不能禁言其他管理员");
    require(minutes >= 0 && minutes <= 1440, "禁言时长为0到1440分钟，0解除");
    if (minutes == 0) g.mutedUntil_.erase(target); else g.mutedUntil_[target] = now() + minutes * 60;
    data_.dirty_ = true;
}
void GroupManagerHsx::disband(int actor, ServiceHsx type, int group) {
    requireManager(actor, type, group, true); const auto related = family(group);
    for (auto it = data_.messages_.begin(); it != data_.messages_.end();) {
        if (it->second.kind() == MessageKindHsx::Group &&
            std::find(related.begin(), related.end(), it->second.target()) != related.end()) it = data_.messages_.erase(it);
        else ++it;
    }
    for (int id : related) data_.groups_.erase(id); data_.dirty_ = true;
}
}
