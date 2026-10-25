#include "AccountScreenHsx.h"
#include "ConsoleHsx.h"
#include "PasswordHsx.h"
#include <iostream>
namespace im {
void AccountScreenHsx::login() {
    const int id = ui_.id("用户ID："); const auto type = ui_.chooseService(id); const auto password = ui_.password("密码：");
    if (ui_.data().user(id).services().size() > 1 && !ui_.confirm("同时登录本人全部已开通服务")) return;
    ui_.accounts().login(id, type, password); ui_.session(id, type); std::cout << "登录成功，全部已开通服务可直接切换。\n";
}
void AccountScreenHsx::registerUser() {
    const int id = ui_.id("用户ID："); const auto name = trim(ui_.text("昵称："));
    const auto birth = trim(ui_.text("出生日期 yyyy-mm-dd：")), location = trim(ui_.text("所在地："));
    const auto type = ui_.chooseService(); const auto number = ui_.number("服务号码：");
    const auto password = ui_.password("密码（至少 8 字节）："); require(password == ui_.password("确认密码："), "两次密码不一致");
    PasswordHsx::check(password);
    const bool shared = false;
    ui_.accounts().registerUser(id, name, birth, location, type, number, password, shared);
    ui_.accounts().login(id, type, password); ui_.session(id, type);
    std::cout << "注册成功，已自动登录。共享服务会同时开通同号QQ。\n";
}
void AccountScreenHsx::services() {
    ui_.requireSession(); ui_.menu("服务管理", {
        {"切换服务", [this] { const auto type = ui_.chooseService(ui_.user()); ui_.session(ui_.user(), type); std::cout << "已切换到 " << serviceName(type) << '\n'; }},
        {"开通服务", [this] {
            const auto type = ui_.chooseService(); const auto number = ui_.number("服务号码：");
            const auto nick = trim(ui_.text("服务昵称：")), password = ui_.password("密码（至少 8 字节）：");
            PasswordHsx::check(password);
            const bool shared = false;
            ui_.accounts().open(ui_.user(), type, number, nick, password, shared); std::cout << "服务已开通。\n"; }},
        {"昵称与密码", [this] {
            const auto nick = trim(ui_.text("新服务昵称：")); const auto password = ui_.password("新密码（至少 8 字节，空保留原密码）："); std::string original;
            if (!password.empty()) { require(password == ui_.password("确认新密码："), "两次密码不一致"); PasswordHsx::check(password); original = ui_.password("原密码："); }
            ui_.accounts().edit(ui_.user(), ui_.service(), nick, original, password); std::cout << "资料已更新。\n"; }},
        {"微信绑定QQ", [this] { const auto number = ui_.number("本人QQ号码（0解除绑定）："); ui_.accounts().bind(ui_.user(), number); std::cout << "绑定已更新。\n"; }},
        {"关闭服务", [this] {
            const auto type = ui_.chooseService(ui_.user()); if (!ui_.confirm("关闭 " + serviceName(type) + " 并清理对应好友和群关系")) return;
            ui_.accounts().close(ui_.user(), type);
            if (ui_.service() == type) ui_.session(ui_.user(), ui_.data().user(ui_.user()).services().begin()->first);
            std::cout << "服务已关闭。\n"; }}
    });
}
void AccountScreenHsx::show() {
    ui_.menu("账号与服务", {
        {"登录 / 切换账号", [this] { login(); }}, {"注册账号", [this] { registerUser(); }},
        {"我的资料", [this] { ui_.requireSession(); ui_.showUser(ui_.user()); }},
        {"修改资料", [this] {
            ui_.requireSession(); const auto name = trim(ui_.text("新昵称：")), location = trim(ui_.text("所在地："));
            ui_.accounts().profile(ui_.user(), name, location); std::cout << "资料已更新。\n"; }},
        {"服务管理", [this] { services(); }},
        {"退出登录", [this] { ui_.requireSession(); ui_.accounts().logout(ui_.user()); ui_.session(0, ServiceHsx::Count); std::cout << "已退出登录。\n"; }}
    });
}
}
