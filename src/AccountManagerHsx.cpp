#include "AccountManagerHsx.h"
#include <limits>
#include "PasswordHsx.h"
#include <stdexcept>
#include "GroupManagerHsx.h"
#include <utility>
namespace im {
void AccountManagerHsx::uniqueNumber(int owner, ServiceHsx type, long long number) const {
    require(activeService(type), "此服务已下线，仅支持 QQ、微信和微博");
    require(number > 0, "服务号码应大于0"); serviceType(static_cast<int>(type));
    for (const auto& item : data_.users()) {
        const auto found = item.second->services().find(type);
        require(item.first == owner || found == item.second->services().end() || found->second.number() != number,
            "此服务号码已经使用");
    }
}
void AccountManagerHsx::registerUser(int id, const std::string& name, const std::string& birth,
    const std::string& location, ServiceHsx primary, long long number, const std::string& password,
    bool shared, const std::string& applied) {
    require(!data_.users_.count(id), "用户ID已经存在"); uniqueName(id,name); uniqueNumber(id, primary, number);
    shared = primary == ServiceHsx::QQ || primary == ServiceHsx::Weibo;
    if (shared && primary != ServiceHsx::QQ) uniqueNumber(id, ServiceHsx::QQ, number);
    const auto type = primary == ServiceHsx::QQ ? "QQ用户" : primary == ServiceHsx::WeChat ? "微信用户" :
        "微博用户";
    auto account = UserHsx::create(type, id, name, DateHsx(birth), DateHsx(applied), location, shared);
    account->services_.emplace(primary, ServiceAccountHsx(primary, number, name, password, shared));
    if (shared && primary != ServiceHsx::QQ)
        account->services_.emplace(ServiceHsx::QQ, ServiceAccountHsx(ServiceHsx::QQ, number, name, password, true));
    data_.users_.emplace(id, std::move(account)); data_.dirty_ = true;
}
void AccountManagerHsx::login(int id, ServiceHsx type, const std::string& password) {
    require(activeService(type), "此服务已下线，仅支持 QQ、微信和微博");
    if(!data_.user(id).loginAllowed()){data_.audit("用户:"+std::to_string(id),"用户登录失败：账号禁止登录",id,type);throw std::runtime_error("此账号已被管理员禁止登录");}
    if(!data_.account(id, type).verify(password)){data_.audit("用户:"+std::to_string(id),"用户登录失败：密码不正确",id,type);throw std::runtime_error("密码不正确");}
    data_.editUser(id).online_ = true;
    data_.audit("用户:" + std::to_string(id), "用户登录", id, type);
}
int AccountManagerHsx::findByNickname(const std::string& nickname) const {
    const auto name=trim(nickname); checkText(name,"昵称"); int found=0;
    for(const auto& [id,user]:data_.users())if(trim(user->name())==name) {
        require(found==0,"此昵称存在重复，请联系管理员核对账号"); found=id;
    }
    require(found!=0,"未找到此昵称，请检查输入或注册账号"); return found;
}
int AccountManagerHsx::availableUserId() const {
    const int largest=data_.users().empty()?10000:data_.users().rbegin()->first;
    require(largest<std::numeric_limits<int>::max()-1,"无法分配新账号，请联系管理员"); return largest+1;
}
void AccountManagerHsx::uniqueName(int owner,const std::string& nickname) const {
    const auto name=trim(nickname);checkText(name,"昵称");
    for(const auto& [id,user]:data_.users())require(id==owner||trim(user->name())!=name,"此昵称已被使用，请换一个昵称");
}
void AccountManagerHsx::logout(int id) { auto& user = data_.editUser(id); if (user.online_) { user.online_ = false; data_.audit("用户:" + std::to_string(id), "用户登出", id); } }
void AccountManagerHsx::profile(int id, const std::string& name, const std::string& location) {
    data_.requireOnline(id); checkText(name, "昵称"); checkText(location, "所在地");
    uniqueName(id,name);
    auto& user = data_.editUser(id); user.name_ = trim(name); for(auto& entry:user.services_)entry.second.nick_=user.name_; user.location_ = location; data_.dirty_ = true;
}
void AccountManagerHsx::open(int id, ServiceHsx type, long long number, const std::string& nick,
    const std::string& password, bool shared) {
    data_.requireOnline(id); uniqueNumber(id, type, number);
    require(!data_.user(id).services().count(type), "该服务已经开通");
    shared = type == ServiceHsx::QQ || type == ServiceHsx::Weibo;
    if (shared && type != ServiceHsx::QQ) require(data_.account(id, ServiceHsx::QQ).number() == number, "共享服务必须使用本人QQ号码");
    data_.editUser(id).services_.emplace(type, ServiceAccountHsx(type, number, nick, password, shared)); data_.dirty_ = true;
}
void AccountManagerHsx::close(int id, ServiceHsx type) {
    data_.requireOnline(id); data_.account(id, type);
    require(data_.user(id).services().size() > 1, "至少保留一个服务");
    if (type == ServiceHsx::QQ) for (const auto& entry : data_.user(id).services())
        require(entry.first == type || !entry.second.shared(), "请先关闭共享QQ号码的服务");
    // 群管理先完成所有关联校验；校验失败时好友、服务和群都保持原样。
    GroupManagerHsx(data_).dropService(id, type);
    for (auto& entry : data_.users_) for (auto& service : entry.second->services_) {
        const auto found = service.second.friends_.find(id);
        if (found != service.second.friends_.end() && found->second.contact() == type) service.second.friends_.erase(found);
    }
    for (auto& entry : data_.requests_) {
        auto& request = entry.second;
        if (request.status_ == RequestStatusHsx::Pending &&
            ((request.from_ == id && request.source_ == type) || (request.to_ == id && request.destination_ == type)))
            request.status_ = RequestStatusHsx::Cancelled;
    }
    auto& user = data_.editUser(id);
    user.services_.erase(type);
    if (type == ServiceHsx::QQ && user.services_.count(ServiceHsx::WeChat)) user.services_.at(ServiceHsx::WeChat).boundQQ_ = 0;
    data_.dirty_ = true;
}
void AccountManagerHsx::edit(int id, ServiceHsx type, const std::string& nick,
    const std::string& currentPassword, const std::string& newPassword) {
    data_.requireOnline(id); checkText(nick, "昵称"); uniqueName(id,nick);
    auto& service = data_.editAccount(id, type);
    if (!newPassword.empty()) {
        require(service.verify(currentPassword), "原密码不正确"); checkText(newPassword, "密码", false, 128);
    }
    const auto hash=newPassword.empty()?std::string{}:PasswordHsx::hash(newPassword,1);
    auto& user=data_.editUser(id);user.name_=trim(nick);for(auto& entry:user.services_)entry.second.nick_=user.name_;
    if (!newPassword.empty()) service.password_ = hash;
    data_.dirty_ = true;
}
void AccountManagerHsx::bind(int id, long long qqNumber) {
    data_.requireOnline(id); auto& wechat = data_.editAccount(id, ServiceHsx::WeChat);
    require(qqNumber == 0 || data_.account(id, ServiceHsx::QQ).number() == qqNumber, "只能绑定本人QQ号");
    wechat.boundQQ_ = qqNumber; data_.dirty_ = true;
}
}
