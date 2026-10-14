#include "StorageHsx.h"
#include "PasswordHsx.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace im {
namespace {
void complete(std::istringstream& input) {
    require(static_cast<bool>(input), "字段缺失或类型错误");
    std::string extra; require(!(input >> extra), "记录含多余字段");
}
template<class T> T number(const std::string& text) {
    T result{}; const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size(), "数字字段无效"); return result;
}
GroupModeHsx mode(int value) {
    require(value == 0 || value == 1, "群类型无效"); return static_cast<GroupModeHsx>(value);
}
}
void StorageHsx::load(DataStoreHsx& data, const std::filesystem::path& path, bool backupOriginal) {
    std::ifstream input(path); require(input.is_open(), "无法读取存档");
    std::string header; std::getline(input, header); header = trim(header); input.close();
    DataStoreHsx staged;
    const bool legacy = header.rfind("USER|", 0) == 0;
    const bool old = legacy || header == "TENCENT 2";
    if (legacy) readLegacy(staged, path);
    else {
        require(header == "TENCENT 2" || header == "TENCENT 3" || header == "TENCENT 4", "未知存档格式，原文件保持原样");
        readModern(staged, path, header == "TENCENT 2" ? 2 : header == "TENCENT 3" ? 3 : 4);
    }
    validate(staged);
    if (old && backupOriginal) {
        auto backup = path; backup += ".original.bak";
        if (!std::filesystem::exists(backup)) std::filesystem::copy_file(path, backup);
    }
    staged.dirty_ = old; data = std::move(staged);
}
void StorageHsx::readModern(DataStoreHsx& data, const std::filesystem::path& path, int version) {
    std::ifstream file(path); require(file.is_open(), "无法读取存档");
    std::string line; std::getline(file, line); size_t row = 1; bool ended = false; std::set<int> statuses;
    while (std::getline(file, line)) {
        ++row; if (trim(line).empty()) continue;
        try {
            require(row <= 1000000 && line.size() <= 32000 && !ended, "存档过大或END后仍有记录");
            std::istringstream in(line); std::string tag; in >> tag;
            if (tag == "USER") {
                int id{}, shared{}; std::string type, name, birth, applied, location;
                in >> id >> std::quoted(type) >> std::quoted(name) >> birth >> applied >> std::quoted(location) >> shared; complete(in);
                require(shared == 0 || shared == 1, "共享标记无效");
                auto account = UserHsx::create(type, id, name, DateHsx(birth), DateHsx(applied), location, shared != 0);
                require(data.users_.emplace(id, std::move(account)).second, "重复用户ID");
            } else if (tag == "SERVICE") {
                int uid{}, type{}, shared{}; long long id{}, bound{}; std::string nick, password;
                in >> uid >> type >> id >> std::quoted(nick) >> std::quoted(password) >> shared >> bound; complete(in);
                require(shared == 0 || shared == 1, "共享标记无效");
                auto selected = serviceType(type); ServiceAccountHsx service(selected, id, nick, password, shared != 0);
                service.boundQQ_ = bound;
                require(data.editUser(uid).services_.emplace(selected, std::move(service)).second, "重复服务");
            } else if (tag == "FRIEND") {
                int uid{}, type{}, target{}, contact{}; std::string remark, category;
                in >> uid >> type >> target >> contact >> std::quoted(remark) >> std::quoted(category); complete(in);
                require(data.editAccount(uid, serviceType(type)).friends_.emplace(target,
                    FriendHsx(target, serviceType(contact), remark, category)).second, "重复好友");
            } else if (tag == "GROUP") {
                int id{}, native{}, policy{}, owner{}, parent{}, capacity{}; std::string name, announcement;
                in >> id >> native >> policy >> owner >> parent >> capacity >> std::quoted(name) >> std::quoted(announcement); complete(in);
                auto group = GroupHsx::create(id, name, owner, mode(native), parent);
                group->policy_ = GroupPolicyHsx::create(mode(policy)); group->capacity_ = capacity;
                checkText(announcement, "群公告", true, 3000); group->announcement_ = announcement;
                require(id < std::numeric_limits<int>::max() && data.groups_.emplace(id, std::move(group)).second, "重复群号或编号过大");
                data.nextGroup_ = std::max(data.nextGroup_, id + 1);
            } else if (tag == "MEMBER" || tag == "PENDING") {
                int gid{}, uid{}, type{}; in >> gid >> uid >> type; complete(in); auto& g = data.editGroup(gid);
                if (tag == "MEMBER") require(g.members_[uid].insert(serviceType(type)).second, "重复成员");
                else require(g.pending_.insert({uid, serviceType(type)}).second, "重复申请");
            } else if (tag == "ADMIN") {
                int gid{}, uid{}; in >> gid >> uid; complete(in); require(data.editGroup(gid).admins_.insert(uid).second, "重复管理员");
            } else if (tag == "MESSAGE") {
                long long id{}; int kind{}, sender{}, target{}, source{}, destination{}, recalled{};
                std::int64_t created{}; std::string text; size_t count{};
                in >> id >> kind >> sender >> target >> source >> destination >> created >> recalled >> std::quoted(text) >> count;
                require(in && id < std::numeric_limits<long long>::max() && count <= data.users_.size() &&
                    (recalled == 0 || recalled == 1), "消息字段无效");
                MessageHsx m(id, static_cast<MessageKindHsx>(kind), sender, target, serviceType(source), serviceType(destination), text, created);
                m.recalled_ = recalled != 0;
                for (size_t i = 0; i < count; ++i) { int reader{}; in >> reader; require(m.readBy_.insert(reader).second, "重复回执"); }
                complete(in); require(data.messages_.emplace(id, std::move(m)).second, "重复消息编号");
                data.nextMessage_ = std::max(data.nextMessage_, id + 1);
            } else if (tag == "END") {
                size_t users{}, groups{}, messages{}, requests{}, logs{}; in >> users >> groups >> messages;
                if (version >= 3) in >> requests;
                if (version == 4) in >> logs;
                complete(in);
                require(users == data.users_.size() && groups == data.groups_.size() && messages == data.messages_.size() &&
                    requests == data.requests_.size() && logs == data.adminLogs_.size(), "记录数量不一致");
                require(version != 4 || !data.adminHash_.empty(), "v4存档缺少管理员配置"); ended = true;
            } else {
                require(version >= 3, "v2存档包含未知记录");
                if (tag == "BLOCK") {
                    int uid{}, type{}, target{}; in >> uid >> type >> target; complete(in);
                    require(data.editAccount(uid, serviceType(type)).blocked_.insert(target).second, "重复黑名单");
                } else if (tag == "MUTE") {
                    int gid{}, uid{}; std::int64_t until{}; in >> gid >> uid >> until; complete(in);
                    require(until > 0 && data.editGroup(gid).mutedUntil_.emplace(uid, until).second, "禁言字段无效");
                } else if (tag == "STAR") {
                    long long id{}; int uid{}; in >> id >> uid; complete(in); data.message(id);
                    require(data.messages_.at(id).stars_.insert(uid).second, "重复收藏");
                } else if (tag == "REQUEST") {
                    long long id{}; int from{}, to{}, source{}, destination{}, status{}; std::string reason;
                    in >> id >> from >> to >> source >> destination >> status >> std::quoted(reason); complete(in);
                    require(status >= 0 && status <= 3 && id < std::numeric_limits<long long>::max(), "申请字段无效");
                    FriendRequestHsx r(id, from, to, serviceType(source), serviceType(destination), reason);
                    r.status_ = static_cast<RequestStatusHsx>(status); require(data.requests_.emplace(id, std::move(r)).second, "重复申请编号");
                    data.nextRequest_ = std::max(data.nextRequest_, id + 1);
                } else if (tag == "ADMINCONFIG" && version == 4) {
                    std::string name, hash; in >> std::quoted(name) >> std::quoted(hash); complete(in);
                    checkText(name, "管理员账号", false, 60); require(PasswordHsx::valid(hash) && data.adminHash_.empty(), "管理员配置重复或密码摘要无效");
                    data.adminName_ = name; data.adminHash_ = hash;
                } else if (tag == "STATUS" && version == 4) {
                    int id{}, disabled{}; in >> id >> disabled; complete(in);
                    require((disabled == 0 || disabled == 1) && statuses.insert(id).second, "用户状态重复或无效"); data.editUser(id).disabled_ = disabled != 0;
                } else if (tag == "ADMINLOG" && version == 4) {
                    long long id{}; std::int64_t created{}; std::string actor, action; int target{}, service{};
                    in >> id >> created >> std::quoted(actor) >> std::quoted(action) >> target >> service; complete(in);
                    require(id < std::numeric_limits<long long>::max() && data.adminLogs_.size() < 2000, "日志编号或数量无效");
                    const auto type = service == static_cast<int>(ServiceHsx::Count) ? ServiceHsx::Count : serviceType(service);
                    require(data.adminLogs_.emplace(id, AdminLogHsx(id, created, actor, action, target, type)).second, "重复管理日志");
                    data.nextAudit_ = std::max(data.nextAudit_, id + 1);
                } else throw std::runtime_error("未知记录：" + tag);
            }
        } catch (const std::exception& error) { throw std::runtime_error("存档第" + std::to_string(row) + "行：" + error.what()); }
    }
    require(file.eof() && ended, "存档未完整写入，原文件保持原样");
}
void StorageHsx::readLegacy(DataStoreHsx& data, const std::filesystem::path& path) {
    std::ifstream file(path); require(file.is_open(), "无法读取原版存档");
    std::vector<std::vector<std::string>> rows; std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        require(rows.size() < 1000000 && line.size() <= 32000, "原版存档过大");
        if (!line.empty()) rows.push_back(split(line, '|'));
    }
    require(file.eof(), "原版存档读取失败");
    auto fields = [](const auto& r, size_t n) { require(r.size() >= n, "原版存档字段缺失"); };
    for (const auto& r : rows) if (r[0] == "USER") {
        fields(r, 8); const int id = number<int>(r[2]);
        require(data.users_.emplace(id, UserHsx::create(r[1], id, r[3], DateHsx(r[4]), DateHsx(r[5]), r[6], r[7] == "1")).second,
            "原版存档有重复用户");
    }
    for (const auto& r : rows) if (r[0] == "SERVICE") {
        fields(r, 7); const int uid = number<int>(r[1]); const auto type = serviceType(r[2]);
        const bool shared = type == ServiceHsx::QQ || type == ServiceHsx::Weibo;
        ServiceAccountHsx account(type, number<long long>(r[3]), r[4], r[5], shared); account.boundQQ_ = number<long long>(r[6]);
        require(data.editUser(uid).services_.emplace(type, std::move(account)).second, "原版存档有重复服务");
    }
    for (auto& entry : data.users_) {
        const auto qq = entry.second->services_.find(ServiceHsx::QQ);
        if (qq == entry.second->services_.end()) continue;
        for (auto& selected : entry.second->services_) if (selected.first >= ServiceHsx::WeiShang)
            selected.second.shared_ = selected.second.number() == qq->second.number();
    }
    for (const auto& r : rows) if (r[0] == "FRIEND") {
        fields(r, 9); require(data.editAccount(number<int>(r[1]), serviceType(r[2])).friends_.emplace(number<int>(r[3]),
            FriendHsx(number<int>(r[3]), serviceType(r[5]), r[7], r[8])).second, "原版存档有重复好友");
    }
    std::map<int, std::vector<int>> members, children;
    for (const auto& r : rows) if (r[0] == "GROUP") {
        fields(r, 8); const int id = number<int>(r[2]); require(r[1] == "QQ群" || r[1] == "微信群", "原版群类型未知");
        auto g = GroupHsx::create(id, r[3], number<int>(r[4]), r[1] == "QQ群" ? GroupModeHsx::QQ : GroupModeHsx::WeChat);
        for (const auto& v : split(r[5], ',')) if (!v.empty()) members[id].push_back(number<int>(v));
        for (const auto& v : split(r[6], ',')) if (!v.empty()) g->admins_.insert(number<int>(v));
        for (const auto& v : split(r[7], ',')) if (!v.empty()) children[id].push_back(number<int>(v));
        require(id < std::numeric_limits<int>::max() && data.groups_.emplace(id, std::move(g)).second, "原版有重复群号或编号过大");
        data.nextGroup_ = std::max(data.nextGroup_, id + 1);
    }
    for (const auto& r : rows) if (r[0] == "SVCGROUP") {
        fields(r, 4); const int uid = number<int>(r[1]), gid = number<int>(r[3]); const auto type = serviceType(r[2]);
        data.account(uid, type); auto& list = members[gid];
        require(std::find(list.begin(), list.end(), uid) != list.end(), "原版入群关系不一致");
        data.editGroup(gid).members_[uid].insert(type);
    }
    for (const auto& entry : members) for (int uid : entry.second) {
        auto& group = data.editGroup(entry.first); if (group.contains(uid)) continue;
        const auto& services = data.user(uid).services(); require(!services.empty(), "原版成员没有服务");
        auto type = group.native() == GroupModeHsx::QQ ? ServiceHsx::QQ : ServiceHsx::WeChat;
        if (!services.count(type)) type = services.begin()->first; group.members_[uid].insert(type);
    }
    for (const auto& entry : children) for (int child : entry.second) {
        auto& g = data.editGroup(child); require(g.parent_ == 0, "讨论组存在多个父群"); g.parent_ = entry.first;
    }
    for (const auto& r : rows) {
        if (r[0] == "MESSAGE") {
            fields(r, 5); const int gid = number<int>(r[1]), uid = number<int>(r[2]);
            std::tm local{}; local.tm_isdst = -1; std::istringstream stamp(r[3]); stamp >> std::get_time(&local, "%Y-%m-%d %H:%M:%S");
            require(!stamp.fail(), "原版消息时间无效"); const auto created = std::mktime(&local);
            auto text = r[4]; for (size_t i = 5; i < r.size(); ++i) text += "|" + r[i];
            const auto& group = data.group(gid); const auto& services = data.user(uid).services(); require(!services.empty(), "消息发送人没有服务");
            const auto type = group.contains(uid) ? *group.members().at(uid).begin() : services.begin()->first;
            const auto id = data.nextMessage_++; data.messages_.emplace(id, MessageHsx(id, MessageKindHsx::Group, uid, gid, type, type, text, created));
        } else require(r[0] == "USER" || r[0] == "SERVICE" || r[0] == "FRIEND" || r[0] == "GROUP" || r[0] == "SVCGROUP", "原版存在未知记录");
    }
}
void StorageHsx::validate(const DataStoreHsx& data) {
    if (!data.adminHash_.empty()) { checkText(data.adminName_, "管理员账号", false, 60); require(PasswordHsx::valid(data.adminHash_), "管理员密码摘要无效"); }
    require(data.adminLogs_.size() <= 2000 && (data.adminLogs_.empty() || !data.adminHash_.empty()), "日志数量或管理员配置无效");
    for (const auto& entry : data.adminLogs_) { require(entry.first == entry.second.id(), "日志编号不一致"); if (entry.second.target()) data.user(entry.second.target()); }
    std::set<std::pair<ServiceHsx, long long>> numbers;
    for (const auto& item : data.users()) {
        const auto& u = *item.second; require(item.first == u.id() && !u.services().empty(), "用户没有服务或ID不一致");
        require(u.loginAllowed() || (!data.adminHash_.empty() && !u.online()), "禁用账号状态无效");
        for (const auto& entry : u.services()) {
            const auto& s = entry.second; require(numbers.insert({entry.first, s.number()}).second, "服务号码重复");
            if (s.shared() && entry.first != ServiceHsx::QQ) require(data.account(u.id(), ServiceHsx::QQ).number() == s.number(), "共享QQ号码不一致");
            if (s.boundQQ()) require(entry.first == ServiceHsx::WeChat && data.account(u.id(), ServiceHsx::QQ).number() == s.boundQQ(), "QQ绑定无效");
            for (const auto& f : s.friends()) { require(f.first != u.id() && !s.blocked().count(f.first), "好友关系无效"); data.account(f.first, f.second.contact()); }
            for (int target : s.blocked()) { require(target != u.id(), "不能屏蔽自己"); data.user(target); }
        }
    }
    for (const auto& entry : data.groups()) {
        const auto& g = *entry.second;
        require(g.contains(g.owner()) && g.capacity() >= 1 && g.capacity() <= 5000 &&
            g.members().size() <= static_cast<size_t>(g.capacity()), "群主或容量无效");
        if (g.parent()) require(!data.group(g.parent()).parent() && g.native() == GroupModeHsx::QQ, "讨论组层级无效");
        for (const auto& member : g.members()) {
            require(!member.second.empty(), "群成员没有服务关联");
            for (auto type : member.second) {
                data.account(member.first, type);
                if (g.parent()) require(data.group(g.parent()).contains(member.first, type), "讨论组成员不在父群");
            }
        }
        for (int admin : g.admins()) require(g.contains(admin), "管理员不在群中");
        for (const auto& pending : g.pending()) {
            data.account(pending.first, pending.second); require(!g.contains(pending.first, pending.second), "重复入群申请");
            if (g.parent()) require(data.group(g.parent()).contains(pending.first, pending.second), "讨论组申请人不在父群");
        }
        for (const auto& mute : g.mutedUntil_) require(g.contains(mute.first), "被禁言成员不在群中");
    }
    for (const auto& entry : data.messages()) {
        const auto& m = entry.second; data.user(m.sender());
        if (m.kind() == MessageKindHsx::Private) { data.user(m.target()); require(m.sender() != m.target(), "私聊不能发给自己"); }
        else data.group(m.target());
        require(m.recalled() ? m.text().empty() : !trim(m.text()).empty(), "消息状态与正文不一致");
        for (const auto& readers : {m.readBy_, m.stars_}) for (int reader : readers) {
            data.user(reader);
            if (m.kind() == MessageKindHsx::Private) require(reader == m.sender() || reader == m.target(), "私聊回执或收藏无效");
        }
        require(!m.recalled() || m.stars_.empty(), "撤回消息不能保留收藏");
    }
    for (const auto& entry : data.requests()) {
        const auto& r = entry.second; data.user(r.from()); data.user(r.to());
        if (r.status() == RequestStatusHsx::Pending) { data.account(r.from(), r.source()); data.account(r.to(), r.destination()); }
    }
}
void StorageHsx::save(DataStoreHsx& data, const std::filesystem::path& path) {
    validate(data); if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    auto temporary = path; temporary += ".tmp"; auto backup = path; backup += ".bak";
    std::ofstream out(temporary, std::ios::trunc); require(out.is_open(), "无法创建临时存档");
    const bool version4 = !data.adminHash_.empty(); out << (version4 ? "TENCENT 4\n" : "TENCENT 3\n");
    if (version4) out << "ADMINCONFIG " << std::quoted(data.adminName_) << ' ' << std::quoted(data.adminHash_) << '\n';
    for (const auto& item : data.users()) {
        const auto& u = *item.second;
        out << "USER " << u.id() << ' ' << std::quoted(u.typeName()) << ' ' << std::quoted(u.name()) << ' '
            << u.birth().str() << ' ' << u.applied().str() << ' ' << std::quoted(u.location()) << ' ' << u.sharesQQ() << '\n';
        if (!u.loginAllowed()) out << "STATUS " << u.id() << " 1\n";
    }
    for (const auto& item : data.users()) for (const auto& entry : item.second->services()) {
        const auto& s = entry.second;
        out << "SERVICE " << item.first << ' ' << static_cast<int>(entry.first) << ' ' << s.number() << ' '
            << std::quoted(s.nick()) << ' ' << std::quoted(s.password_) << ' ' << s.shared() << ' ' << s.boundQQ() << '\n';
        for (const auto& f : s.friends()) out << "FRIEND " << item.first << ' ' << static_cast<int>(entry.first) << ' ' << f.first << ' '
            << static_cast<int>(f.second.contact()) << ' ' << std::quoted(f.second.remark()) << ' ' << std::quoted(f.second.category()) << '\n';
        for (int target : s.blocked()) out << "BLOCK " << item.first << ' ' << static_cast<int>(entry.first) << ' ' << target << '\n';
    }
    for (const auto& item : data.groups()) {
        const auto& g = *item.second;
        out << "GROUP " << g.id() << ' ' << static_cast<int>(g.native()) << ' ' << static_cast<int>(g.policy().mode()) << ' ' << g.owner()
            << ' ' << g.parent() << ' ' << g.capacity() << ' ' << std::quoted(g.name()) << ' ' << std::quoted(g.announcement()) << '\n';
    }
    for (const auto& item : data.groups()) {
        const auto& g = *item.second;
        for (const auto& member : g.members()) for (auto type : member.second) out << "MEMBER " << g.id() << ' ' << member.first << ' ' << static_cast<int>(type) << '\n';
        for (int admin : g.admins()) out << "ADMIN " << g.id() << ' ' << admin << '\n';
        for (const auto& pending : g.pending()) out << "PENDING " << g.id() << ' ' << pending.first << ' ' << static_cast<int>(pending.second) << '\n';
        for (const auto& mute : g.mutedUntil_) if (mute.second > now()) out << "MUTE " << g.id() << ' ' << mute.first << ' ' << mute.second << '\n';
    }
    for (const auto& item : data.messages()) {
        const auto& m = item.second;
        out << "MESSAGE " << m.id() << ' ' << static_cast<int>(m.kind()) << ' ' << m.sender() << ' ' << m.target() << ' '
            << static_cast<int>(m.source()) << ' ' << static_cast<int>(m.destination()) << ' ' << m.created() << ' ' << m.recalled()
            << ' ' << std::quoted(m.text()) << ' ' << m.readBy_.size();
        for (int reader : m.readBy_) out << ' ' << reader; out << '\n';
        for (int owner : m.stars_) out << "STAR " << m.id() << ' ' << owner << '\n';
    }
    for (const auto& item : data.requests()) {
        const auto& r = item.second;
        out << "REQUEST " << r.id() << ' ' << r.from() << ' ' << r.to() << ' ' << static_cast<int>(r.source()) << ' '
            << static_cast<int>(r.destination()) << ' ' << static_cast<int>(r.status()) << ' ' << std::quoted(r.reason()) << '\n';
    }
    for (const auto& entry : data.adminLogs_) { const auto& log = entry.second; out << "ADMINLOG " << log.id() << ' ' << log.created() << ' ' << std::quoted(log.actor()) << ' ' << std::quoted(log.action()) << ' ' << log.target() << ' ' << static_cast<int>(log.service()) << '\n'; }
    out << "END " << data.users().size() << ' ' << data.groups().size() << ' ' << data.messages().size() << ' ' << data.requests().size();
    if (version4) out << ' ' << data.adminLogs_.size(); out << '\n';
    out.close(); require(!out.fail(), "临时存档写入失败");
    if (std::filesystem::exists(path)) std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing);
#ifdef _WIN32
    require(MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0, "替换存档失败");
#else
    std::filesystem::rename(temporary, path);
#endif
    data.dirty_ = false;
}
}
