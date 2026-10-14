#include "AdminManagerHsx.h"
#include "PasswordHsx.h"
#include <stdexcept>
namespace im {
void AdminManagerHsx::initialize() {
    if (!data_.adminHash_.empty()) return;
    data_.adminHash_ = PasswordHsx::hash("admin123456");
    data_.audit(data_.adminName_, "初始化管理员");
}
void AdminManagerHsx::login(const std::string& name, const std::string& password) {
    require(!data_.adminHash_.empty(), "管理员尚未初始化");
    if(name != data_.adminName_ || !PasswordHsx::verify(password,data_.adminHash_)){data_.audit(data_.adminName_,"管理员登录失败");throw std::runtime_error("管理员账号或密码不正确");}
    data_.adminOnline_ = true; data_.audit(data_.adminName_, "管理员登录");
}
void AdminManagerHsx::logout() {
    if (!active()) return;
    data_.adminOnline_ = false; data_.audit(data_.adminName_, "管理员登出");
}
void AdminManagerHsx::requireAdmin() const { require(active(), "需要系统管理员登录，群管理员不具备此权限"); }
void AdminManagerHsx::setLoginAllowed(int user, bool allowed) {
    requireAdmin(); auto& target = data_.editUser(user);
    target.disabled_ = !allowed; if (!allowed) target.online_ = false;
    data_.audit(data_.adminName_, allowed ? "允许用户登录" : "禁止用户登录并登出", user);
}
void AdminManagerHsx::forceLogout(int user) {
    requireAdmin(); data_.editUser(user).online_ = false;
    data_.audit(data_.adminName_, "强制用户登出", user);
}
void AdminManagerHsx::resetPassword(int user, ServiceHsx service, const std::string& password) {
    requireAdmin(); checkText(password, "新密码", false, 128); auto& target = data_.editUser(user);
    if (service == ServiceHsx::Count) { for (auto& entry : target.services_) entry.second.password_ = PasswordHsx::hash(password,1); }
    else data_.editAccount(user, service).password_ = PasswordHsx::hash(password,1);
    target.online_ = false;
    data_.audit(data_.adminName_, service == ServiceHsx::Count ? "重置全部服务密码并登出" : "重置服务密码并登出", user, service);
}
void AdminManagerHsx::changePassword(const std::string& original, const std::string& password) {
    requireAdmin(); require(PasswordHsx::verify(original, data_.adminHash_), "原管理员密码不正确");
    const auto hash = PasswordHsx::hash(password); data_.adminHash_ = hash;
    data_.audit(data_.adminName_, "修改管理员密码");
}
}
