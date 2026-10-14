#include "GroupScreenHsx.h"
#include "ConsoleHsx.h"
#include "MessageScreenHsx.h"
#include <iostream>
namespace im {
void GroupScreenHsx::list(bool mine, const std::string& keyword) {
    size_t count = 0;
    for (const auto& entry : ui_.data().groups()) {
        const auto& g = *entry.second;
        if (mine && !g.contains(ui_.user(), ui_.service())) continue;
        if (!keyword.empty() && (g.name() + std::to_string(g.id())).find(keyword) == std::string::npos) continue;
        std::cout << g.id() << ' ' << g.name() << " [" << modeName(g.policy().mode()) << "模式] " << g.members().size() << '/' << g.capacity();
        if (g.parent()) std::cout << " / 父群 " << g.parent();
        if (g.pending().count({ui_.user(), ui_.service()})) std::cout << " [已申请]";
        std::cout << '\n'; ++count;
    }
    std::cout << "共 " << count << " 个群。\n";
}
void GroupScreenHsx::details(int id) {
    ui_.groups().requireMember(ui_.user(), ui_.service(), id); const auto& g = ui_.data().group(id);
    std::cout << g.id() << ' ' << g.name() << " / 群主 " << g.owner() << " / " << modeName(g.policy().mode()) << "模式\n"
        << g.policy().description() << "\n公告：" << (g.announcement().empty() ? "暂无" : g.announcement()) << '\n';
    for (const auto& entry : g.members()) {
        std::cout << entry.first << ' ' << ui_.data().user(entry.first).name();
        if (g.owner() == entry.first) std::cout << " [群主]"; else if (g.manages(entry.first)) std::cout << " [管理员]";
        if (g.muted(entry.first)) std::cout << " [禁言中]";
        for (auto service : entry.second) std::cout << ' ' << serviceName(service); std::cout << '\n';
    }
}
void GroupScreenHsx::manage(int id) {
    const auto& group = ui_.data().group(id); require(group.manages(ui_.user()), "没有群管理权限");
    std::vector<ConsoleHsx::Action> actions{
        {"入群审批", [this, id] {
            const auto& g = ui_.data().group(id); require(g.policy().applications(), "当前模式仅允许邀请入群");
            for (const auto& request : g.pending()) std::cout << request.first << ' ' << ui_.data().user(request.first).name() << " / " << serviceName(request.second) << '\n';
            if (g.pending().empty()) { std::cout << "没有待审批申请。\n"; return; }
            const int user = ui_.id("申请人ID（0返回）："); if (!user) return; const auto type = ui_.chooseService(user);
            const auto choice = ui_.number("1同意 / 2拒绝 / 0返回："); if (!choice) return; require(choice == 1 || choice == 2, "选项无效");
            ui_.groups().approve(ui_.user(), ui_.service(), id, user, type, choice == 1); std::cout << "申请已处理。\n"; }},
        {"群资料与公告", [this, id] {
            const auto name = trim(ui_.text("新群名：")), announcement = ui_.text("群公告（可空）："); const int capacity = ui_.id("人数上限：");
            ui_.groups().edit(ui_.user(), ui_.service(), id, name, announcement, capacity); std::cout << "群资料已更新。\n"; }},
        {"移除成员", [this, id] {
            const int target = ui_.id("成员ID："); if (ui_.confirm("移除该成员及其对应讨论组关系")) { ui_.groups().kick(ui_.user(), ui_.service(), id, target); std::cout << "成员已移除。\n"; } }},
        {"禁言管理", [this, id] {
            const int target = ui_.id("成员ID："), minutes = ui_.id("禁言分钟数（0解除 / 最大1440）：");
            ui_.groups().mute(ui_.user(), ui_.service(), id, target, minutes); std::cout << "禁言设置已更新。\n"; }}
    };
    if (group.owner() == ui_.user()) {
        actions.push_back({"管理员设置", [this, id] {
            const int target = ui_.id("成员ID："); const auto value = ui_.number("1设为管理员 / 2取消 / 0返回："); if (!value) return;
            require(value == 1 || value == 2, "选项无效"); ui_.groups().admin(ui_.user(), ui_.service(), id, target, value == 1); std::cout << "管理员已更新。\n"; }});
        actions.push_back({"转让群主", [this, id] {
            const int target = ui_.id("新群主ID："); if (ui_.confirm("转让群主权限")) { ui_.groups().transfer(ui_.user(), ui_.service(), id, target); std::cout << "已转让。\n"; } }});
        actions.push_back({"切换群模式", [this, id] {
            const auto mode = ui_.chooseMode(); ui_.groups().mode(ui_.user(), ui_.service(), id, mode); std::cout << "模式已切换，成员、申请、讨论组与消息保持完整。\n"; }});
        actions.push_back({"解散群组", [this, id] {
            if (ui_.confirm("解散本群、所属讨论组及其聊天记录")) { ui_.groups().disband(ui_.user(), ui_.service(), id); std::cout << "群组已解散。\n"; } }});
    }
    ui_.menu("群管理", actions, [this, id] {
        const auto found = ui_.data().groups().find(id);
        return found != ui_.data().groups().end() && found->second->contains(ui_.user(), ui_.service()) && found->second->manages(ui_.user());
    });
}
void GroupScreenHsx::selected(int id) {
    ui_.groups().requireMember(ui_.user(), ui_.service(), id); details(id);
    ui_.menu("群 " + std::to_string(id), {
        {"成员与公告", [this, id] { details(id); }},
        {"连续群聊", [this, id] { MessageScreenHsx(ui_).chat(MessageKindHsx::Group, id); }},
        {"邀请成员", [this, id] {
            const int target = ui_.id("目标用户ID："); const auto type = ui_.chooseService(target);
            ui_.groups().invite(ui_.user(), ui_.service(), id, target, type); std::cout << "成员已加入。\n"; }},
        {"群管理", [this, id] { manage(id); }},
        {"创建讨论组", [this, id] {
            const auto name = trim(ui_.text("讨论组名称：")); const int created = ui_.groups().create(ui_.user(), ui_.service(), name, GroupModeHsx::QQ, id);
            std::cout << "讨论组已创建，群号 " << created << '\n'; }},
        {"退出群组", [this, id] {
            if (ui_.confirm("退出当前服务在本群和讨论组的成员关系")) { ui_.groups().leave(ui_.user(), ui_.service(), id); std::cout << "已退出。\n"; } }}
    }, [this, id] {
        const auto found = ui_.data().groups().find(id); return found != ui_.data().groups().end() && found->second->contains(ui_.user(), ui_.service());
    });
}
void GroupScreenHsx::show() {
    ui_.requireSession(); ui_.menu("群组管理", {
        {"我的群组", [this] { list(true); }},
        {"查找群组", [this] { list(false, trim(ui_.text("群名 / 群号（空查看全部）："))); }},
        {"进入群组", [this] { selected(ui_.id("群号：")); }},
        {"创建群组", [this] {
            const auto name = trim(ui_.text("群名：")); const auto mode = ui_.chooseMode(); const int id = ui_.groups().create(ui_.user(), ui_.service(), name, mode);
            std::cout << "群组已创建，群号 " << id << '\n'; }},
        {"申请入群", [this] { const int id = ui_.id("群号："); ui_.groups().apply(ui_.user(), ui_.service(), id); std::cout << "申请已提交，等待审批。\n"; }},
        {"我的入群申请", [this] {
            for (const auto& entry : ui_.data().groups()) if (entry.second->pending().count({ui_.user(), ui_.service()})) std::cout << entry.first << ' ' << entry.second->name() << " [待审批]\n";
            const int id = ui_.id("取消申请的群号（0返回）："); if (id) { ui_.groups().cancelApplication(ui_.user(), ui_.service(), id); std::cout << "已取消。\n"; } }}
    });
}
}
