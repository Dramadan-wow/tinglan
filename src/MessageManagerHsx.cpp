#include "MessageManagerHsx.h"
#include "FriendManagerHsx.h"
#include "GroupManagerHsx.h"
#include <fstream>
#include <limits>
namespace im {
long long MessageManagerHsx::append(MessageKindHsx kind, int actor, int target, ServiceHsx source, ServiceHsx destination, const std::string& text) {
    checkText(text, "消息", false, 6000); require(data_.nextMessage_ < std::numeric_limits<long long>::max(), "消息编号已用尽");
    const auto id = data_.nextMessage_++;
    auto result = data_.messages_.emplace(id, MessageHsx(id, kind, actor, target, source, destination, text, now()));
    result.first->second.readBy_.insert(actor); data_.dirty_ = true; return id;
}
long long MessageManagerHsx::sendPrivate(int actor, ServiceHsx type, int target, const std::string& text) {
    data_.requireOnline(actor); const auto& friends = data_.account(actor, type).friends();
    const auto found = friends.find(target); require(found != friends.end(), "请先添加对方为当前服务好友");
    require(FriendManagerHsx(data_).allowed(actor, type, target, found->second.contact()), "黑名单阻止了发送");
    return append(MessageKindHsx::Private, actor, target, type, found->second.contact(), text);
}
long long MessageManagerHsx::sendGroup(int actor, ServiceHsx type, int group, const std::string& text, int mention) {
    GroupManagerHsx(data_).requireMember(actor, type, group); const auto& g = data_.group(group);
    require(!g.muted(actor), "你目前被禁言，请等待解除"); checkText(text, "消息", false, 5500);
    std::string content = text;
    if (mention == -1) { require(g.manages(actor), "只有群主或管理员可以@全体"); content = "@全体 " + text; }
    else if (mention != 0) { require(g.contains(mention), "被@的用户不在群中"); content = "@" + data_.user(mention).name() + " " + text; }
    return append(MessageKindHsx::Group, actor, group, type, type, content);
}
bool MessageManagerHsx::visible(int actor, ServiceHsx type, const MessageHsx& m) const {
    if (m.kind() == MessageKindHsx::Group) {
        const auto found = data_.groups().find(m.target());
        return found != data_.groups().end() && found->second->contains(actor, type);
    }
    return (m.sender() == actor && m.source() == type) || (m.target() == actor && m.destination() == type);
}
std::vector<const MessageHsx*> MessageManagerHsx::inbox(int actor, ServiceHsx type, bool onlyUnread, const std::string& keyword) const {
    data_.requireOnline(actor); data_.account(actor, type); std::vector<const MessageHsx*> result;
    for (const auto& entry : data_.messages()) {
        const auto& m = entry.second;
        if (!visible(actor, type, m)) continue;
        if (onlyUnread && (m.read(actor) || m.recalled() || m.sender() == actor)) continue;
        if (!keyword.empty() && (m.recalled() || m.text().find(keyword) == std::string::npos)) continue;
        result.push_back(&m);
    }
    return result;
}
std::vector<const MessageHsx*> MessageManagerHsx::history(int actor, ServiceHsx type,
    MessageKindHsx kind, int target, const std::string& keyword) const {
    if (kind == MessageKindHsx::Group) GroupManagerHsx(data_).requireMember(actor, type, target);
    else data_.user(target);
    std::vector<const MessageHsx*> result;
    for (auto m : inbox(actor, type, false, keyword))
        if (m->kind() == kind && (kind == MessageKindHsx::Group ? m->target() == target :
            (m->sender() == target || m->target() == target))) result.push_back(m);
    return result;
}
std::vector<const MessageHsx*> MessageManagerHsx::favorites(int actor, ServiceHsx type) const {
    std::vector<const MessageHsx*> result;
    for (auto message : inbox(actor, type)) if (message->starred(actor) && !message->recalled()) result.push_back(message);
    return result;
}
void MessageManagerHsx::markRead(int actor, ServiceHsx type, long long id) {
    data_.requireOnline(actor); data_.account(actor, type); const auto& m = data_.message(id);
    require(visible(actor, type, m), "无权读取此消息");
    if (data_.messages_.at(id).readBy_.insert(actor).second) data_.dirty_ = true;
}
void MessageManagerHsx::recall(int actor, long long id) {
    data_.requireOnline(actor); const auto& m = data_.message(id); const auto age = now() - m.created();
    require(m.sender() == actor && !m.recalled() && age >= 0 && age <= 120, "仅可在发送后2分钟内撤回本人消息");
    auto& message = data_.messages_.at(id); message.recalled_ = true; message.text_.clear(); message.stars_.clear(); data_.dirty_ = true;
}
void MessageManagerHsx::star(int actor, ServiceHsx type, long long id, bool enabled) {
    data_.requireOnline(actor); data_.account(actor, type); const auto& m = data_.message(id);
    require(visible(actor, type, m) && !m.recalled(), "消息不可访问或已经撤回");
    if (enabled) data_.messages_.at(id).stars_.insert(actor); else data_.messages_.at(id).stars_.erase(actor); data_.dirty_ = true;
}
void MessageManagerHsx::exportHistory(int actor, ServiceHsx type, MessageKindHsx kind,
    int target, const std::filesystem::path& path) const {
    const auto records = history(actor, type, kind, target);
    require(!std::filesystem::exists(path), "目标文件已存在，请换一个文件名");
    std::ofstream out(path, std::ios::binary); require(out.is_open(), "无法创建导出文件");
    out << "\xEF\xBB\xBF" << "聊天记录 / " << serviceName(type) << " / 导出于 " << timeText(now()) << "\r\n";
    for (const auto* m : records) out << '#' << m->id() << " [" << timeText(m->created()) << "] " << data_.user(m->sender()).name()
        << "：" << (m->recalled() ? "[消息已撤回]" : m->text()) << "\r\n";
    out.close(); require(!out.fail(), "导出写入失败");
}
}
