#include "ServiceAccountHsx.h"
#include <utility>
#include "PasswordHsx.h"
namespace im {
ServiceAccountHsx::ServiceAccountHsx(ServiceHsx type, long long number, std::string nick, std::string password, bool shared)
    : type_(type), number_(number), nick_(std::move(nick)), password_(std::move(password)), shared_(shared) {
    serviceType(static_cast<int>(type)); require(number > 0, "服务号码应大于0");
    if(!PasswordHsx::valid(password_))password_=PasswordHsx::hash(password_,1);
    checkText(nick_, "服务昵称"); checkText(password_, "密码", false, 128);
    require(type != ServiceHsx::QQ && type != ServiceHsx::Weibo || shared, "QQ与微博必须共享号码");
    require(type != ServiceHsx::WeChat || !shared, "微信应采用独立号码");
}
bool ServiceAccountHsx::verify(const std::string& password)const{return PasswordHsx::verify(password,password_);}
}

