#include "FriendManagerHsx.h"
#include <limits>
namespace im {
bool FriendManagerHsx::allowed(int actor, ServiceHsx source, int target, ServiceHsx destination) const {
    return !data_.account(actor, source).blocked().count(target) && !data_.account(target, destination).blocked().count(actor);
}
long long FriendManagerHsx::request(int actor, ServiceHsx source, int target, ServiceHsx destination, const std::string& reason) {
    data_.requireOnline(actor); require(actor != target, "不能添加自己"); checkText(reason, "验证信息", true);
    require(allowed(actor, source, target, destination), "黑名单阻止了本次申请");
    require(!data_.account(actor, source).friends().count(target) && !data_.account(target, destination).friends().count(actor),
        "已有好友关系，请先检查双方好友列表");
    for (const auto& entry : data_.requests()) {
        const auto& r = entry.second;
        if (r.status() != RequestStatusHsx::Pending) continue;
        const bool same = r.from() == actor && r.to() == target && r.source() == source && r.destination() == destination;
        const bool reverse = r.from() == target && r.to() == actor && r.source() == destination && r.destination() == source;
        require(!same && !reverse, "已有待处理申请，请到申请中心处理");
    }
    require(data_.nextRequest_ < std::numeric_limits<long long>::max(), "申请编号已用尽");
    const auto id = data_.nextRequest_++;
    data_.requests_.emplace(id, FriendRequestHsx(id, actor, target, source, destination, reason)); data_.dirty_ = true;
    return id;
}
void FriendManagerHsx::respond(int actor, ServiceHsx type, long long id, bool accept) {
    data_.requireOnline(actor); data_.account(actor, type);
    const auto found = data_.requests_.find(id); require(found != data_.requests_.end(), "好友申请不存在");
    auto& r = found->second;
    require(r.to_ == actor && r.destination_ == type && r.status_ == RequestStatusHsx::Pending, "无权处理或申请已经处理");
    if (accept) {
        require(allowed(r.from_, r.source_, r.to_, r.destination_), "黑名单阻止了好友关系");
        auto& sender = data_.editAccount(r.from_, r.source_); auto& receiver = data_.editAccount(actor, type);
        require(!sender.friends_.count(actor) && !receiver.friends_.count(r.from_), "好友关系冲突，请拒绝此申请并检查好友列表");
        sender.friends_.emplace(actor, FriendHsx(actor, type, "", "我的好友"));
        receiver.friends_.emplace(r.from_, FriendHsx(r.from_, r.source_, "", "我的好友"));
    }
    r.status_ = accept ? RequestStatusHsx::Accepted : RequestStatusHsx::Rejected; data_.dirty_ = true;
}
void FriendManagerHsx::cancel(int actor, long long id) {
    data_.requireOnline(actor); auto found = data_.requests_.find(id);
    require(found != data_.requests_.end() && found->second.from() == actor && found->second.status() == RequestStatusHsx::Pending,
        "只能取消本人待处理申请");
    found->second.status_ = RequestStatusHsx::Cancelled; data_.dirty_ = true;
}
void FriendManagerHsx::edit(int actor, ServiceHsx type, int target, const std::string& remark, const std::string& category) {
    data_.requireOnline(actor); checkText(remark, "备注", true); checkText(category, "分组");
    auto& friends = data_.editAccount(actor, type).friends_;
    require(friends.count(target) != 0, "此用户不是当前服务好友");
    friends.at(target).remark_ = remark; friends.at(target).category_ = category; data_.dirty_ = true;
}
void FriendManagerHsx::remove(int actor, ServiceHsx type, int target) {
    data_.requireOnline(actor); auto& friends = data_.editAccount(actor, type).friends_;
    const auto found = friends.find(target); require(found != friends.end(), "好友不存在");
    auto& other = data_.editAccount(target, found->second.contact()).friends_;
    const auto reverse = other.find(actor);
    if (reverse != other.end() && reverse->second.contact() == type) other.erase(reverse);
    friends.erase(found); data_.dirty_ = true;
}
void FriendManagerHsx::block(int actor, ServiceHsx type, int target, bool enabled) {
    data_.requireOnline(actor); data_.user(target); require(actor != target, "不能将自己加入黑名单");
    auto& service = data_.editAccount(actor, type);
    if (enabled) {
        if (service.friends_.count(target)) remove(actor, type, target);
        service.blocked_.insert(target);
        for (auto& entry : data_.requests_) {
            auto& r = entry.second;
            if (r.status_ == RequestStatusHsx::Pending &&
                ((r.from_ == actor && r.source_ == type && r.to_ == target) || (r.to_ == actor && r.destination_ == type && r.from_ == target)))
                r.status_ = RequestStatusHsx::Cancelled;
        }
    } else service.blocked_.erase(target);
    data_.dirty_ = true;
}
std::vector<int> FriendManagerHsx::common(int actor, ServiceHsx first, ServiceHsx second) const {
    data_.requireOnline(actor); require(first != second, "请选择另一项服务");
    const auto& left = data_.account(actor, first).friends(); const auto& right = data_.account(actor, second).friends();
    std::vector<int> result;
    for (const auto& entry : left) if (right.count(entry.first)) result.push_back(entry.first);
    return result;
}
std::vector<int> FriendManagerHsx::recommend(int actor, ServiceHsx destination, ServiceHsx source) const {
    data_.requireOnline(actor); require(destination != source, "请选择另一项服务");
    const auto& target = data_.account(actor, destination); std::vector<int> result;
    for (const auto& entry : data_.account(actor, source).friends())
        if (!target.friends().count(entry.first) && !target.blocked().count(entry.first)) result.push_back(entry.first);
    return result;
}
}
