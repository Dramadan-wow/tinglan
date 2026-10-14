#include "AccountManagerHsx.h"
#include "FriendManagerHsx.h"
#include "GroupManagerHsx.h"
#include "MessageManagerHsx.h"
#include "StorageHsx.h"
#include "DemoDataHsx.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace im;
namespace {
int checks = 0;
void expect(bool condition, const std::string& description) {
    ++checks; if (!condition) throw std::runtime_error("FAILED: " + description);
}
template<class F> void rejects(F action, const std::string& description) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    expect(rejected, description);
}
std::string contents(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary); std::ostringstream out; out << in.rdbuf(); return out.str();
}
}
int main() {
    try {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto output = std::filesystem::u8path(__FILE__).parent_path() / "output" / ("run-" + std::to_string(stamp));
        std::filesystem::create_directories(output);
        const auto qq = ServiceHsx::QQ, wc = ServiceHsx::WeChat, wb = ServiceHsx::Weibo;
        rejects([] { DateHsx("2025-02-29"); }, "invalid leap day");
        rejects([] { DateHsx("2026-13-01"); }, "invalid month");
        rejects([] { DateHsx("2000-aa-01"); }, "invalid date text");
        expect(DateHsx("2024-02-29").str() == "2024-02-29", "valid leap date");
        expect(DateHsx("2000-10-09").yearsUntil(DateHsx("2026-10-08")) == 25, "anniversary age");
        expect(DateHsx("2000-10-08").yearsUntil(DateHsx("2026-10-08")) == 26, "birthday age");
        rejects([] { serviceType(7); }, "invalid service enum");
        rejects([] { checkText("\n", "message"); }, "control characters");
        rejects([] { checkText("   ", "message"); }, "blank message");
        expect(serviceType("WeChat") == wc && serviceName(wb) == "微博", "service names");

        DataStoreHsx data; AccountManagerHsx a(data); FriendManagerHsx f(data); GroupManagerHsx g(data); MessageManagerHsx m(data);
        for (int id = 1; id <= 6; ++id) {
            a.registerUser(id, "用户" + std::to_string(id), "2000-01-01", "中国", qq, 10000 + id, "123456"); a.login(id, qq, "123456");
        }
        expect(data.users().size() == 6, "registration");
        rejects([&] { a.registerUser(1, "重复", "2000-01-01", "中国", qq, 10011, "123456"); }, "duplicate user");
        rejects([&] { a.registerUser(7, "重复号", "2000-01-01", "中国", qq, 10001, "123456"); }, "duplicate number");
        rejects([&] { a.registerUser(7, "未来", "9999-01-01", "中国", qq, 10007, "123456"); }, "future birth");
        expect(!data.users().count(7), "failed registration atomic");
        rejects([&] { a.login(1, qq, "wrong"); }, "login password");
        a.logout(1); rejects([&] { a.profile(1, "非法", "中国"); }, "offline mutation"); a.login(1, qq, "123456");
        a.open(1, wc, 40001, "微信一", "123456"); a.open(2, wc, 40002, "微信二", "123456"); a.open(3, wc, 40003, "微信三", "123456");
        a.open(1, wb, 10001, "微博一", "123456");
        expect(data.account(1, wb).shared(), "weibo shares QQ");
        rejects([&] { a.open(2, wb, 33333, "错误", "123456"); }, "wrong shared number");
        a.bind(1, 10001); expect(data.account(1, wc).boundQQ() == 10001, "QQ binding");
        rejects([&] { a.bind(1, 10002); }, "foreign QQ binding");
        a.bind(1, 0); expect(data.account(1, wc).boundQQ() == 0, "unbind");
        rejects([&] { a.close(1, qq); }, "shared QQ dependency");
        rejects([&] { a.close(6, qq); }, "last service protected");
        rejects([&] { a.edit(1, qq, "用户一", "bad", "new"); }, "old password verified");
        expect(data.account(1, qq).nick() == "用户1", "password update atomic");
        a.edit(1, qq, "用户一", "123456", "new-password"); a.logout(1); a.login(1, qq, "new-password");
        expect(data.user(1).online(), "new password works");
        a.profile(1, "新昵称", "长春"); expect(data.user(1).name() == "新昵称", "edit profile");
        a.registerUser(7, "微博新用户", "2000-01-01", "中国", wb, 10007, "123456");
        expect(data.user(7).typeName() == "微博用户" && data.account(7, qq).number() == 10007, "shared registration auto QQ");
        rejects([&]{ a.registerUser(8, "已下线服务", "2000-01-01", "中国", ServiceHsx::WeiShang, 50008, "123456"); }, "retired registration rejected");
        for(int retired=3;retired<7;++retired)rejects([&]{a.open(1,ServiceHsx(retired),50008,"下线服务","123456");},"retired service open rejected");
        a.registerUser(8, "独立微信", "2000-01-01", "中国", wc, 50008, "123456");
        expect(!data.user(8).sharesQQ() && data.user(8).services().size() == 1, "independent WeChat service");

        auto req = f.request(1, qq, 2, qq, "中文 \"验证\" | \\信息");
        expect(data.requests().at(req).status() == RequestStatusHsx::Pending && data.account(1, qq).friends().empty(), "pending request");
        rejects([&] { f.request(2, qq, 1, qq, "反向申请"); }, "reverse duplicate request");
        rejects([&] { f.respond(3, qq, req, true); }, "wrong approver");
        rejects([&] { f.respond(2, wc, req, true); }, "wrong approving service");
        f.respond(2, qq, req, true);
        expect(data.account(1, qq).friends().count(2) && data.account(2, qq).friends().count(1), "mutual friendship");
        rejects([&] { f.respond(2, qq, req, true); }, "request handled once");
        rejects([&] { f.request(1, qq, 1, qq, ""); }, "self friendship");
        f.edit(1, qq, 2, "同学", "课程组"); expect(data.account(1, qq).friends().at(2).category() == "课程组", "remark and category");
        req = f.request(1, wc, 2, wc, "微信同学"); f.respond(2, wc, req, true);
        expect(f.common(1, qq, wc) == std::vector<int>{2}, "common friend across services");
        req = f.request(1, qq, 3, qq, "三号好友"); f.respond(3, qq, req, true);
        expect(f.recommend(1, wc, qq) == std::vector<int>{3}, "cross service recommendation");
        req = f.request(4, qq, 5, qq, "取消"); f.cancel(4, req);
        expect(data.requests().at(req).status() == RequestStatusHsx::Cancelled, "request cancellation");
        req = f.request(4, qq, 5, qq, "拒绝"); f.respond(5, qq, req, false);
        expect(data.requests().at(req).status() == RequestStatusHsx::Rejected, "request rejection");
        req = f.request(4, qq, 5, qq, "屏蔽"); f.block(5, qq, 4, true);
        expect(data.requests().at(req).status() == RequestStatusHsx::Cancelled, "block cancels request");
        rejects([&] { f.request(4, qq, 5, qq, ""); }, "block denies request"); f.block(5, qq, 4, false);
        req = f.request(4, qq, 5, qq, "再申请"); f.respond(5, qq, req, true);
        f.block(5, qq, 4, true);
        expect(!data.account(4, qq).friends().count(5) && !data.account(5, qq).friends().count(4), "block removes mutual friendship");
        f.block(5, qq, 4, false);

        const int root = g.create(1, qq, "课程大群", GroupModeHsx::QQ); expect(root == 1001, "initial group number");
        g.invite(1, qq, root, 2, qq); g.invite(1, qq, root, 1, wc);
        expect(data.group(root).members().size() == 2, "membership counted by user");
        g.apply(3, qq, root); expect(!data.group(root).contains(3), "application needs approval");
        rejects([&] { g.approve(2, qq, root, 3, qq, true); }, "nonadmin cannot approve");
        g.admin(1, qq, root, 2, true); g.approve(2, qq, root, 3, qq, true);
        expect(data.group(root).contains(3, qq), "admin approval");
        rejects([&] { g.admin(2, qq, root, 3, true); }, "only owner sets admins");
        rejects([&] { g.kick(2, qq, root, 1); }, "owner kick protected");
        g.edit(2, qq, root, "课程大群", "中文公告 | \\" , 3);
        rejects([&] { g.invite(1, qq, root, 4, qq); }, "capacity enforced");
        rejects([&] { g.edit(1, qq, root, "变更", "", 2); }, "cannot shrink under member count");
        expect(data.group(root).name() == "课程大群", "group edit atomic");
        const int child = g.create(1, qq, "讨论组", GroupModeHsx::QQ, root); g.invite(1, qq, child, 2, qq);
        rejects([&] { g.invite(1, qq, child, 4, qq); }, "child subset enforced");
        rejects([&] { g.create(1, qq, "孙群", GroupModeHsx::QQ, child); }, "two level hierarchy");
        g.apply(3, qq, child); g.mode(1, qq, root, GroupModeHsx::WeChat);
        expect(data.group(root).members().size() == 3 && data.group(child).pending().size() == 1, "policy switch preserves data");
        expect(!data.group(root).manages(2), "wechat suspends admins");
        rejects([&] { g.apply(4, qq, root); }, "wechat rejects application");
        rejects([&] { g.create(1, qq, "微信讨论组", GroupModeHsx::QQ, root); }, "wechat disallows subgroup");
        rejects([&] { g.edit(2, qq, root, "非法", "", 3); }, "suspended admin denied");
        g.mode(1, qq, root, GroupModeHsx::QQ); expect(data.group(root).manages(2), "QQ restores admins");
        g.leave(3, qq, root); expect(!data.group(root).contains(3) && data.group(child).pending().empty(), "parent leave clears child application");
        g.edit(1, qq, root, "课程大群", "公告", 500); g.invite(1, qq, root, 3, qq);
        g.mute(1, qq, root, 3, 10); rejects([&] { m.sendGroup(3, qq, root, "禁言消息"); }, "mute denies send");
        rejects([&] { g.mute(2, qq, root, 1, 1); }, "cannot mute owner");
        g.mute(1, qq, root, 3, 0); expect(!data.group(root).muted(3), "unmute");
        rejects([&] { m.sendGroup(3, qq, root, "消息", -1); }, "all mention privilege");
        const auto groupMessage = m.sendGroup(2, qq, root, "群聊正文", -1);
        expect(data.message(groupMessage).text().find("@全体") == 0, "all mention");
        rejects([&] { m.sendGroup(4, qq, root, "外部消息"); }, "nonmember cannot send");
        rejects([&] { m.sendGroup(2, qq, root, "消息", 6); }, "mention member required");

        const auto privateMessage = m.sendPrivate(1, qq, 2, "中文 | \"引号\" \\反斜杠");
        expect(!data.message(privateMessage).read(2), "recipient initially unread");
        rejects([&] { m.sendPrivate(1, qq, 6, "陌生人"); }, "stranger private denied");
        rejects([&] { m.markRead(6, qq, privateMessage); }, "private read access");
        rejects([&] { m.markRead(2, wc, privateMessage); }, "destination service enforced");
        expect(m.history(2, wc, MessageKindHsx::Private, 1).empty(), "service histories isolated");
        expect(m.inbox(2, qq, true).size() == 1, "unread recipient only");
        m.markRead(2, qq, privateMessage); expect(data.message(privateMessage).read(2), "read receipt");
        m.star(2, qq, privateMessage, true); expect(m.favorites(2, qq).size() == 1, "favorites");
        rejects([&] { m.star(6, qq, privateMessage, true); }, "favorite access");
        const auto exportFile = output / std::filesystem::u8path("中文记录.txt");
        m.exportHistory(2, qq, MessageKindHsx::Private, 1, exportFile);
        expect(contents(exportFile).find("中文 |") != std::string::npos, "Unicode export");
        rejects([&] { m.exportHistory(2, qq, MessageKindHsx::Private, 1, exportFile); }, "export prevents overwrite");
        rejects([&] { m.recall(2, privateMessage); }, "only sender recalls");
        m.recall(1, privateMessage);
        expect(data.message(privateMessage).text().empty() && m.favorites(2, qq).empty(), "recall removes body and stars");
        expect(m.inbox(2, qq, false, "引号").empty(), "recalled text unsearchable");
        rejects([&] { m.star(2, qq, privateMessage, true); }, "recalled cannot favorite");
        rejects([&] { m.recall(1, privateMessage); }, "recall once");
        f.block(2, qq, 1, true); rejects([&] { m.sendPrivate(1, qq, 2, "屏蔽后"); }, "blocked sending"); f.block(2, qq, 1, false);
        expect(m.history(2, qq, MessageKindHsx::Private, 1).size() == 1, "history retained after unfriend");

        const int loneChild = g.create(1, qq, "孤立讨论组", GroupModeHsx::QQ, root);
        rejects([&] { g.leave(1, qq, root); }, "owner cannot orphan lone child");
        expect(data.group(root).contains(1, qq) && data.group(child).contains(1, qq), "leave fails atomically");
        rejects([&] { a.close(1, qq); }, "close service validates dependencies");
        g.disband(1, qq, loneChild); g.leave(2, qq, root);
        expect(!data.group(child).contains(2) && !data.group(root).admins().count(2), "cascade removes permissions");
        g.invite(1, qq, root, 2, qq); g.invite(1, qq, child, 2, qq);
        g.kick(1, qq, root, 2); expect(!data.group(child).contains(2), "kick cascades");
        g.leave(1, wc, root); expect(data.group(root).contains(1, qq), "leaving one service preserves other");
        const int lone = g.create(4, qq, "最后群主", GroupModeHsx::QQ);
        rejects([&] { g.leave(4, qq, lone); }, "last owner cannot leave"); g.disband(4, qq, lone);
        g.mute(1, qq, root, 3, 30); g.transfer(1, qq, root, 3);
        expect(data.group(root).owner() == 3 && !data.group(root).muted(3), "transfer unmutes new owner");

        // 关闭服务时清理群、请求和外部好友引用；失败不能造成部分删除。
        const int wcGroup = g.create(2, wc, "微信群", GroupModeHsx::WeChat);
        g.invite(2, wc, wcGroup, 1, wc); req = f.request(3, wc, 2, wc, "待取消");
        a.close(2, wc);
        expect(data.group(wcGroup).owner() == 1 && !data.group(wcGroup).contains(2), "closing service transfers owner");
        expect(!data.account(1, wc).friends().count(2), "closing service clears incoming friends");
        expect(data.requests().at(req).status() == RequestStatusHsx::Cancelled, "closing service cancels request");

        const int protectedServiceGroup = g.create(1, wc, "关闭服务保护", GroupModeHsx::WeChat);
        const auto serviceCount = data.user(1).services().size();
        rejects([&] { a.close(1, wc); }, "last owner prevents closing service");
        expect(data.user(1).services().size() == serviceCount && data.group(protectedServiceGroup).contains(1, wc), "closing service failure atomic");
        g.disband(1, wc, protectedServiceGroup);
        g.mute(3, qq, root, 1, 30); g.mode(3, qq, root, GroupModeHsx::WeChat);
        m.star(1, qq, groupMessage, true); f.block(5, qq, 4, true);
        f.request(6, qq, 5, qq, "重启后处理");

        const auto saveFile = output / "state.txt"; StorageHsx::save(data, saveFile);
        expect(!data.dirty() && contents(saveFile).rfind("TENCENT 3", 0) == 0, "new format save");
        DataStoreHsx loaded; StorageHsx::load(loaded, saveFile);
        expect(loaded.users().size() == data.users().size() && loaded.groups().size() == data.groups().size(), "save load round trip");
        expect(!loaded.user(1).online(), "login sessions not persisted");
        expect(loaded.message(privateMessage).recalled() && loaded.requests().size() == data.requests().size(), "message request persistence");
        expect(loaded.group(root).policy().mode() == GroupModeHsx::WeChat && loaded.group(root).muted(1), "policy and mute persistence");
        expect(loaded.message(groupMessage).starred(1) && loaded.account(5, qq).blocked().count(4), "favorites and blacklist persistence");
        a.profile(1, "再修改", "长春"); StorageHsx::save(data, saveFile);
        auto previousBackup = saveFile; previousBackup += ".bak";
        expect(std::filesystem::exists(previousBackup), "previous backup");
        const auto prior = contents(saveFile); a.profile(1, "待保存", "长春");
        auto tempFile = saveFile; tempFile += ".tmp"; std::filesystem::create_directory(tempFile);
        rejects([&] { StorageHsx::save(data, saveFile); }, "failed save reported");
        expect(contents(saveFile) == prior && data.dirty(), "failed save preserves main data and dirty flag");
        const auto corrupt = output / "corrupt.txt";
        { std::ofstream out(corrupt); out << "TENCENT 3\nUSER invalid\n"; }
        const auto corruption = contents(corrupt);
        rejects([&] { StorageHsx::load(loaded, corrupt); }, "corrupt save refused");
        expect(loaded.users().size() == 8 && contents(corrupt) == corruption, "failed load preserves memory and disk");
        { std::ofstream out(corrupt); out << "TENCENT 99\n"; }
        rejects([&] { StorageHsx::load(loaded, corrupt); }, "unknown version refused");
        { std::ofstream out(corrupt); out << prior.substr(0, prior.find("END ")); }
        rejects([&] { StorageHsx::load(loaded, corrupt); }, "truncation refused");
        const auto fixture = std::filesystem::u8path(__FILE__).parent_path() / "fixtures" / "legacy.txt";
        const auto old = output / "legacy.txt"; std::filesystem::copy_file(fixture, old);
        DataStoreHsx migrated; StorageHsx::load(migrated, old); expect(migrated.users().size() == 2 && migrated.group(1002).parent() == 1001, "legacy migration");
        expect(migrated.group(1002).contains(10002, qq), "legacy missing membership restored");
        auto original = old; original += ".original.bak"; expect(contents(original) == contents(fixture), "migration exact original backup");
        StorageHsx::save(migrated, old); expect(contents(old).rfind("TENCENT 3", 0) == 0, "legacy upgraded format");
        const auto v2 = output / "v2.txt";
        { std::ofstream out(v2); out << "TENCENT 2\nUSER 1 \"QQ用户\" \"旧用户\" 2000-01-01 2020-01-01 \"中国\" 1\nSERVICE 1 0 1 \"旧用户\" \"123456\" 1 0\nEND 1 0 0\n"; }
        DataStoreHsx v2Data; StorageHsx::load(v2Data, v2, false);
        auto v2Backup = v2; v2Backup += ".original.bak";
        expect(v2Data.user(1).name() == "旧用户" && !std::filesystem::exists(v2Backup), "v2 import no source mutation");
        DataStoreHsx demo; DemoDataHsx::create(demo); StorageHsx::validate(demo);
        expect(demo.users().size() == 4 && demo.groups().size() == 7 && demo.messages().size() == 2, "complete demo data");
        AccountManagerHsx nicknames(demo);
        expect(nicknames.findByNickname("  张三  ")==10001,"trimmed nickname resolves account");
        rejects([&]{nicknames.findByNickname("10001");},"internal ID is not nickname");
        rejects([&]{nicknames.findByNickname("不存在");},"unknown nickname rejected");
        expect(nicknames.availableUserId()==10005,"new user ID automatically allocated");
        rejects([&]{nicknames.registerUser(10005,"张三","2000-01-01","长春",qq,90901,"new-pass");},"duplicate nickname registration rejected");
        expect(demo.users().size()==4,"duplicate nickname registration atomic");
        nicknames.login(10002,qq,"123456");rejects([&]{nicknames.profile(10002,"张三","北京");},"duplicate nickname rename rejected");
        expect(demo.user(10002).name()=="李四","failed nickname rename preserves profile");
        nicknames.profile(10002,"清禾","长春");expect(nicknames.findByNickname("清禾")==10002,"renamed nickname resolves original account");
        rejects([&]{nicknames.findByNickname("李四");},"old nickname no longer resolves renamed user");
        const auto renamed=output/"nickname.txt";StorageHsx::save(demo,renamed);DataStoreHsx afterRename;StorageHsx::load(afterRename,renamed,false);AccountManagerHsx renamedAccounts(afterRename);
        expect(renamedAccounts.findByNickname("清禾")==10002,"nickname login survives restart");
        g.disband(3, qq, root); expect(!data.groups().count(child) && !data.messages().count(groupMessage), "disband cascade and history cleanup");
        std::cout << "PASS " << checks << " core checks\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
