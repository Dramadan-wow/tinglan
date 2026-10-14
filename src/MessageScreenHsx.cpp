#include "MessageScreenHsx.h"
#include "ConsoleHsx.h"
#include <charconv>
#include <iostream>
#include <limits>
#include <sstream>
namespace im {
void MessageScreenHsx::chat(MessageKindHsx kind, int target) {
    const auto records = ui_.messages().history(ui_.user(), ui_.service(), kind, target);
    const auto label = kind == MessageKindHsx::Group ? ui_.data().group(target).name() : ui_.data().user(target).name();
    std::cout << "\n聊天：" << label << " / 输入正文直接发送，/help查看命令，/back返回\n";
    // 仅显示最近8条，并只给显示过的消息生成已读回执。
    const size_t first = records.size() > 8 ? records.size() - 8 : 0;
    for (size_t i = first; i < records.size(); ++i) {
        const auto& m = *records[i]; std::cout << '#' << m.id() << ' ' << ui_.data().user(m.sender()).name() << "：" << (m.recalled() ? "[消息已撤回]" : m.text()) << '\n';
        ui_.messages().markRead(ui_.user(), ui_.service(), m.id());
    }
    ui_.save();
    while (true) {
        try {
            const auto line = ui_.text("消息> "); if (trim(line).empty()) continue;
            if (line == "/help") { std::cout << "/back返回 /history记录 /find 关键词 /recall 编号 /star 编号 /unstar 编号\n群聊还支持 /at 用户ID 正文（ID=-1为全体）\n"; continue; }
            if (line == "/history") { ui_.showMessages(ui_.messages().history(ui_.user(), ui_.service(), kind, target)); continue; }
            if (line.rfind("/find ", 0) == 0) { ui_.showMessages(ui_.messages().history(ui_.user(), ui_.service(), kind, target, line.substr(6))); continue; }
            if (line.rfind("/recall ", 0) == 0 || line.rfind("/star ", 0) == 0 || line.rfind("/unstar ", 0) == 0) {
                const auto separator = line.find(' '); const auto value = trim(line.substr(separator + 1)); long long id{};
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), id);
                require(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size(), "消息编号无效");
                const auto command = line.substr(0, separator);
                if (command == "/recall") ui_.messages().recall(ui_.user(), id);
                else ui_.messages().star(ui_.user(), ui_.service(), id, command == "/star");
                std::cout << "消息状态已更新。\n";
            } else if (line.rfind("/at ", 0) == 0) {
                require(kind == MessageKindHsx::Group, "@命令仅用于群聊");
                std::istringstream input(line.substr(4)); int mention{}; input >> mention; require(static_cast<bool>(input), "格式为 /at 用户ID 正文");
                std::string body; std::getline(input, body); const auto id = ui_.messages().sendGroup(ui_.user(), ui_.service(), target, trim(body), mention);
                std::cout << "已发送 #" << id << '\n';
            } else {
                require(line.front() != '/', "未知命令，输入/help查看");
                const auto id = kind == MessageKindHsx::Group ? ui_.messages().sendGroup(ui_.user(), ui_.service(), target, line) :
                    ui_.messages().sendPrivate(ui_.user(), ui_.service(), target, line);
                std::cout << "已发送 #" << id << '\n';
            }
            ui_.save();
        } catch (const std::ios_base::failure&) { throw; }
        catch (const std::exception& error) { if (std::string(error.what()) == "已取消") return; std::cout << "提示：" << error.what() << '\n'; }
    }
}
void MessageScreenHsx::show() {
    ui_.menu("消息中心", {
        {"未读消息", [this] { ui_.showMessages(ui_.messages().inbox(ui_.user(), ui_.service(), true)); }},
        {"连续私聊", [this] { chat(MessageKindHsx::Private, ui_.id("好友用户ID：")); }},
        {"连续群聊", [this] { chat(MessageKindHsx::Group, ui_.id("群号：")); }},
        {"历史与搜索", [this] { ui_.showMessages(ui_.messages().inbox(ui_.user(), ui_.service(), false, ui_.text("关键词（空查看全部）："))); }},
        {"我的收藏", [this] { ui_.showMessages(ui_.messages().favorites(ui_.user(), ui_.service())); }},
        {"收藏 / 取消收藏", [this] { const auto id = ui_.number("消息编号："); const auto choice = ui_.number("1收藏 / 2取消 / 0返回："); if (!choice) return;
            require(choice == 1 || choice == 2, "选项无效"); ui_.messages().star(ui_.user(), ui_.service(), id, choice == 1); std::cout << "收藏已更新。\n"; }},
        {"撤回消息", [this] { const auto id = ui_.number("本人消息编号："); ui_.messages().recall(ui_.user(), id); std::cout << "消息已撤回。\n"; }},
        {"导出聊天记录", [this] {
            const auto choice = ui_.number("1私聊 / 2群聊 / 0返回："); if (!choice) return; require(choice == 1 || choice == 2, "选项无效");
            const int target = ui_.id(choice == 1 ? "对方用户ID：" : "群号：");
            const auto name = trim(ui_.text("导出文件名（如聊天记录.txt）：")); const auto requested = std::filesystem::u8path(name);
            require(!name.empty() && !requested.is_absolute() && requested == requested.filename() && name != "." && name != "..", "请输入单个文件名");
            const auto directory = ui_.file().parent_path() / "exports"; std::filesystem::create_directories(directory); const auto path = directory / requested;
            ui_.messages().exportHistory(ui_.user(), ui_.service(), choice == 1 ? MessageKindHsx::Private : MessageKindHsx::Group, target, path);
            std::cout << "已导出：" << path.u8string() << '\n'; }}
    });
}
}
