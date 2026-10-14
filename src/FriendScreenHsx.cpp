#include "FriendScreenHsx.h"
#include "ConsoleHsx.h"
#include <algorithm>
#include <iostream>
namespace im {
void FriendScreenHsx::list(const std::string& keyword) {
    size_t count = 0;
    for (const auto& entry : ui_.data().account(ui_.user(), ui_.service()).friends()) {
        const auto& f = entry.second; const auto& u = ui_.data().user(entry.first);
        if (!keyword.empty() && (u.name() + std::to_string(u.id()) + f.remark() + f.category()).find(keyword) == std::string::npos) continue;
        std::cout << u.id() << "  " << u.name() << " / " << serviceName(f.contact()) << ':' << ui_.data().account(u.id(), f.contact()).number()
            << " / " << f.category() << " / 备注:" << f.remark() << '\n'; ++count;
    }
    std::cout << "共 " << count << " 位好友。\n";
}
void FriendScreenHsx::sendRequest(int target) {
    const auto type = ui_.chooseService(target); const auto reason = ui_.text("验证信息（可空）：");
    const auto id = ui_.friends().request(ui_.user(), ui_.service(), target, type, reason); std::cout << "好友申请已发送，编号 " << id << "。对方同意后建立双向关系。\n";
}
void FriendScreenHsx::requests() {
    ui_.menu("好友申请", {
        {"收到的申请", [this] {
            size_t count = 0;
            for (const auto& entry : ui_.data().requests()) {
                const auto& r = entry.second;
                if (r.to() == ui_.user() && r.destination() == ui_.service() && r.status() == RequestStatusHsx::Pending) {
                    std::cout << '#' << r.id() << "  " << r.from() << ' ' << ui_.data().user(r.from()).name() << " / " << serviceName(r.source()) << " / " << r.reason() << '\n'; ++count;
                }
            }
            if (!count) { std::cout << "没有待处理好友申请。\n"; return; }
            const auto id = ui_.number("申请编号（0返回）："); if (!id) return;
            const auto choice = ui_.number("1同意 / 2拒绝 / 0返回："); if (!choice) return; require(choice == 1 || choice == 2, "选项无效");
            ui_.friends().respond(ui_.user(), ui_.service(), id, choice == 1); std::cout << "申请已处理。\n"; }},
        {"发出的申请", [this] {
            static const char* states[] = {"待处理", "已同意", "已拒绝", "已取消"};
            for (const auto& entry : ui_.data().requests()) {
                const auto& r = entry.second;
                if (r.from() == ui_.user() && r.source() == ui_.service()) std::cout << '#' << r.id() << " → " << r.to() << ' '
                    << ui_.data().user(r.to()).name() << " / " << serviceName(r.destination()) << " / " << states[static_cast<int>(r.status())] << '\n';
            }
            const auto id = ui_.number("取消待处理申请编号（0返回）："); if (id) { ui_.friends().cancel(ui_.user(), id); std::cout << "已取消。\n"; } }}
    });
}
void FriendScreenHsx::recommend() {
    const auto source = ui_.chooseService(ui_.user()); const auto ids = ui_.friends().recommend(ui_.user(), ui_.service(), source); ui_.showUsers(ids);
    if (ids.empty()) return; const int target = ui_.id("推荐用户ID（0返回）："); if (!target) return;
    require(std::find(ids.begin(), ids.end(), target) != ids.end(), "请选择推荐名单中的用户"); sendRequest(target);
}
void FriendScreenHsx::show() {
    ui_.requireSession(); ui_.menu("好友管理", {
        {"好友列表与搜索", [this] { list(trim(ui_.text("昵称 / ID / 备注 / 分组（空查看全部）："))); }},
        {"添加好友", [this] { sendRequest(ui_.id("目标用户ID：")); }},
        {"申请中心", [this] { requests(); }},
        {"备注与分组", [this] {
            const int target = ui_.id("好友用户ID："); const auto remark = ui_.text("备注（可空）："), category = trim(ui_.text("分组："));
            ui_.friends().edit(ui_.user(), ui_.service(), target, remark, category); std::cout << "好友信息已更新。\n"; }},
        {"删除好友", [this] { const int target = ui_.id("好友用户ID："); if (ui_.confirm("解除对应双方好友关系")) { ui_.friends().remove(ui_.user(), ui_.service(), target); std::cout << "好友已删除。\n"; } }},
        {"跨服务推荐", [this] { recommend(); }},
        {"共同好友", [this] { const auto other = ui_.chooseService(ui_.user()); ui_.showUsers(ui_.friends().common(ui_.user(), ui_.service(), other)); }},
        {"黑名单", [this] {
            for (int id : ui_.data().account(ui_.user(), ui_.service()).blocked()) std::cout << id << ' ' << ui_.data().user(id).name() << '\n';
            const int target = ui_.id("用户ID（0返回）："); if (!target) return;
            const auto action = ui_.number("1屏蔽 / 2解除 / 0返回："); if (!action) return; require(action == 1 || action == 2, "选项无效");
            if (action == 1 && !ui_.confirm("屏蔽用户并解除对应好友关系")) return;
            ui_.friends().block(ui_.user(), ui_.service(), target, action == 1); std::cout << "黑名单已更新。\n"; }}
    });
}
}
