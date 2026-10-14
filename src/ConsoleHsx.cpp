#include "ConsoleHsx.h"
#include "AccountScreenHsx.h"
#include "AdminScreenHsx.h"
#include "FriendScreenHsx.h"
#include "GroupScreenHsx.h"
#include "MessageScreenHsx.h"
#include "NetChatHsx.h"
#include "StorageHsx.h"
#include <algorithm>
#include <charconv>
#include <iostream>
#include <limits>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace im {
ConsoleHsx::ConsoleHsx(DataStoreHsx& data, std::filesystem::path file)
    : data_(data), file_(std::move(file)), accounts_(data), friends_(data), groups_(data), messages_(data) {}
std::string ConsoleHsx::text(const std::string& prompt) {
    std::cout << prompt << std::flush; std::string value;
    if (!std::getline(std::cin, value)) throw std::ios_base::failure("输入结束");
    if (trim(value) == "/back") throw std::runtime_error("已取消");
    return value;
}
std::string ConsoleHsx::password(const std::string& prompt) {
#ifdef _WIN32
    const HANDLE handle = GetStdHandle(STD_INPUT_HANDLE); DWORD mode{};
    const bool hidden = GetConsoleMode(handle, &mode) && SetConsoleMode(handle, mode & ~ENABLE_ECHO_INPUT);
    try {
        auto value = text(prompt);
        if (hidden) { SetConsoleMode(handle, mode); std::cout << '\n'; }
        return value;
    } catch (...) { if (hidden) { SetConsoleMode(handle, mode); std::cout << '\n'; } throw; }
#else
    return text(prompt);
#endif
}
long long ConsoleHsx::number(const std::string& prompt) {
    while (true) {
        const auto value = trim(text(prompt)); long long result{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
        if (parsed.ec == std::errc() && parsed.ptr == value.data() + value.size()) return result;
        std::cout << "请输入完整整数。\n";
    }
}
int ConsoleHsx::id(const std::string& prompt) {
    const auto value = number(prompt);
    require(value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max(), "数字超出范围"); return static_cast<int>(value);
}
bool ConsoleHsx::confirm(const std::string& prompt) {
    while (true) { const auto value = number(prompt + "（1确认 / 0取消）："); if (value == 0 || value == 1) return value == 1; }
}
ServiceHsx ConsoleHsx::chooseService(int user) {
    std::vector<ServiceHsx> choices;
    if (user) { for (const auto& entry : data_.user(user).services()) if (activeService(entry.first)) choices.push_back(entry.first); }
    else for (int i = 0; i < ActiveServiceCountHsx; ++i) choices.push_back(serviceType(i));
    for (size_t i = 0; i < choices.size(); ++i) std::cout << i + 1 << '.' << serviceName(choices[i]) << "  ";
    std::cout << "0.取消\n"; const auto choice = number("服务：");
    require(choice > 0 && static_cast<size_t>(choice) <= choices.size(), "已取消或服务选择无效"); return choices[static_cast<size_t>(choice - 1)];
}
GroupModeHsx ConsoleHsx::chooseMode() {
    const auto choice = number("群模式（1 QQ / 2 微信 / 0取消）：");
    require(choice == 1 || choice == 2, "已取消或群模式无效"); return choice == 1 ? GroupModeHsx::QQ : GroupModeHsx::WeChat;
}
void ConsoleHsx::requireSession() const { require(user_ != 0, "请先到账号服务中登录"); data_.requireOnline(user_); data_.account(user_, service_); }
void ConsoleHsx::session(int user, ServiceHsx type) {
    if (user_ && user_ != user) accounts_.logout(user_); user_ = user; service_ = type;
}
void ConsoleHsx::save() {
    if (!data_.dirty()) return;
    try { StorageHsx::save(data_, file_); }
    catch (const std::exception& error) { std::cout << "[保存失败] " << error.what() << "。内存数据仍保留，请检查路径后重试。\n"; }
}
void ConsoleHsx::menu(const std::string& title, const std::vector<Action>& actions, const std::function<bool()>& guard) {
    while (!guard || guard()) {
        std::cout << "\n[" << title << "]\n";
        for (size_t i = 0; i < actions.size(); ++i) std::cout << i + 1 << "." << actions[i].first << (i % 2 == 0 && i + 1 < actions.size() ? "    " : "\n");
        std::cout << "0.返回\n"; const auto choice = number("选择：");
        if (choice == 0) return;
        if (choice < 1 || static_cast<size_t>(choice) > actions.size()) { std::cout << "没有这个选项。\n"; continue; }
        try { actions[static_cast<size_t>(choice - 1)].second(); }
        catch (const std::ios_base::failure&) { throw; }
        catch (const std::exception& error) { std::cout << "提示：" << error.what() << '\n'; }
        save();
    }
}
void ConsoleHsx::showUser(int id) const {
    const auto& u = data_.user(id);
    std::cout << u.id() << "  " << u.name() << " / " << u.location() << " / " << u.typeName()
        << "\n出生 " << u.birth().str() << " / 申请 " << u.applied().str() << " / T龄 " << u.tAge() << "年\n";
    for (const auto& entry : u.services()) {
        const auto& s = entry.second;
        std::cout << "  " << serviceName(entry.first) << ':' << s.number() << "  " << s.nick();
        if (s.shared()) std::cout << " [共享QQ号]";
        if (s.boundQQ()) std::cout << " [绑定QQ:" << s.boundQQ() << ']'; std::cout << '\n';
    }
}
void ConsoleHsx::showUsers(const std::vector<int>& users) const {
    for (int id : users) std::cout << id << "  " << data_.user(id).name() << '\n';
    std::cout << "共 " << users.size() << " 位用户。\n";
}
void ConsoleHsx::showMessages(const std::vector<const MessageHsx*>& messages, bool markRead) {
    if (messages.empty()) { std::cout << "暂无符合条件的消息。\n"; return; }
    const size_t size = 8, pages = (messages.size() + size - 1) / size;
    size_t page = 0;
    while (true) {
        const size_t last = messages.size() - page * size;
        const size_t first = last > size ? last - size : 0;
        for (size_t i = first; i < last; ++i) {
            const auto& m = *messages[i];
            std::cout << '#' << m.id() << " [" << timeText(m.created()) << "] " << data_.user(m.sender()).name()
                << (m.kind() == MessageKindHsx::Group ? " → 群" : " → 用户") << m.target() << "\n  "
                << (m.recalled() ? "[消息已撤回]" : m.text());
            if (!m.recalled() && m.sender() == user_ && m.kind() == MessageKindHsx::Private) std::cout << (m.read(m.target()) ? " [已读]" : " [未读]");
            if (m.starred(user_)) std::cout << " [已收藏]"; std::cout << '\n';
            if (markRead) messages_.markRead(user_, service_, m.id());
        }
        save(); std::cout << "第 " << pages - page << '/' << pages << " 页 / 共 " << messages.size() << " 条\n";
        const auto choice = number("1更早 / 2更新 / 0返回：");
        if (choice == 0) return;
        if (choice == 1 && page + 1 < pages) ++page;
        else if (choice == 2 && page > 0) --page;
        else std::cout << "没有这一页。\n";
    }
}
void ConsoleHsx::status() const {
    if (!user_) { std::cout << "未登录 | 演示账号10001到10004，密码123456，10003选微信\n"; return; }
    size_t groups = 0, requests = 0, approvals = 0;
    for (const auto& entry : data_.groups()) {
        const auto& g = *entry.second;
        if (g.contains(user_, service_)) { ++groups; if (g.manages(user_) && g.policy().applications()) approvals += g.pending().size(); }
    }
    for (const auto& entry : data_.requests()) if (entry.second.to() == user_ && entry.second.destination() == service_ && entry.second.status() == RequestStatusHsx::Pending) ++requests;
    std::cout << data_.user(user_).name() << '(' << user_ << ") / " << serviceName(service_) << " | 好友 "
        << data_.account(user_, service_).friends().size() << " / 群 " << groups << " / 未读 " << messages_.inbox(user_, service_, true).size()
        << " / 好友申请 " << requests << " / 入群审批 " << approvals << '\n';
}
void ConsoleHsx::tools() {
    menu("工具与数据", {
        {"用户目录", [this] {
            const auto keyword = trim(text("昵称 / 用户ID / 服务号关键词（空查看全部）：")); size_t count = 0;
            for (const auto& entry : data_.users()) {
                auto haystack = std::to_string(entry.first) + entry.second->name() + entry.second->location();
                for (const auto& s : entry.second->services()) haystack += std::to_string(s.second.number());
                if (keyword.empty() || haystack.find(keyword) != std::string::npos) { showUser(entry.first); ++count; }
            }
            std::cout << "共 " << count << " 位用户。\n"; }},
        {"平台统计", [this] {
            size_t services = 0, links = 0, pending = 0, privateCount = 0, groupCount = 0, recalled = 0;
            for (const auto& entry : data_.users()) for (const auto& s : entry.second->services()) { ++services; links += s.second.friends().size(); }
            for (const auto& entry : data_.requests()) if (entry.second.status() == RequestStatusHsx::Pending) ++pending;
            for (const auto& entry : data_.messages()) { if (entry.second.kind() == MessageKindHsx::Private) ++privateCount; else ++groupCount; if (entry.second.recalled()) ++recalled; }
            std::cout << "用户 " << data_.users().size() << " / 服务 " << services << " / 好友引用 " << links << " / 群 " << data_.groups().size()
                << "\n私聊 " << privateCount << " / 群消息 " << groupCount << " / 已撤回 " << recalled << " / 待处理好友申请 " << pending << '\n'; }},
        {"保存数据", [this] { StorageHsx::save(data_, file_); std::cout << "保存成功：" << file_.u8string() << '\n'; }},
        {"TCP点对点通信", [this] {
            menu("TCP通信", {
                {"监听", [this] { const int port = id("端口："); NetChatHsx::chat(true, "", port, user_ ? data_.user(user_).name() : "服务端"); }},
                {"连接", [this] { const auto ip = trim(text("对方IPv4：")); const int port = id("端口："); NetChatHsx::chat(false, ip, port, user_ ? data_.user(user_).name() : "客户端"); }}
            }); }},
        {"课程规则", [] { std::cout << "QQ、微信、微博三种服务 / 微博共享QQ号 / 微信独立号可绑定QQ\nQQ支持审批、管理员、讨论组；微信仅邀请入群和群主管理\n切换群策略保留成员与历史；业务修改自动保存；聊天中输入/help查看命令\n"; }}
    });
}
void ConsoleHsx::run() {
    std::cout << "\n===== 模拟即时通信系统 v4 =====\n输入数字选择；表单输入 /back 可取消。\n";
    try {
        menu("主菜单 · 0保存退出", {
            {"账号与服务", [this] { AccountScreenHsx(*this).show(); }},
            {"好友与群组", [this] { requireSession(); menu("社交", {{"好友管理", [this] { FriendScreenHsx(*this).show(); }}, {"群组管理", [this] { GroupScreenHsx(*this).show(); }}}); }},
            {"消息中心", [this] { requireSession(); MessageScreenHsx(*this).show(); }},
            {"工具与数据", [this] { tools(); }},
            {"系统管理员", [this] { AdminScreenHsx(*this).show(); }}
        }, [this] { if(user_&&!data_.user(user_).online())session(0,ServiceHsx::Count); status(); return true; });
    } catch (const std::ios_base::failure&) { std::cout << "\n输入结束。\n"; }
    if(user_) { accounts_.logout(user_); session(0,ServiceHsx::Count); }
    save(); std::cout << (data_.dirty() ? "数据尚未保存，请保留当前窗口中的错误信息。\n" : "数据已保存，再见。\n");
}
}
