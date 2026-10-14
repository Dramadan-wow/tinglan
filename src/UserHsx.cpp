#include "UserHsx.h"
#include "QQUserHsx.h"
#include "WeChatUserHsx.h"
#include "WeiboUserHsx.h"
#include "OtherUserHsx.h"
#include <utility>
namespace im {
UserHsx::UserHsx(int id, std::string name, DateHsx birth, DateHsx applied, std::string location)
    : id_(id), name_(std::move(name)), location_(std::move(location)), birth_(birth), applied_(applied) {
    require(id > 0, "用户ID应大于0"); checkText(name_, "昵称"); checkText(location_, "所在地");
    require(!(applied < birth) && !(DateHsx::today() < applied), "申请日期不能早于出生日期或晚于今天");
}
std::unique_ptr<UserHsx> UserHsx::create(const std::string& type, int id, const std::string& name,
    const DateHsx& birth, const DateHsx& applied, const std::string& location, bool shared) {
    if (type == "QQ用户") return std::make_unique<QQUserHsx>(id, name, birth, applied, location);
    if (type == "微信用户") return std::make_unique<WeChatUserHsx>(id, name, birth, applied, location);
    if (type == "微博用户") return std::make_unique<WeiboUserHsx>(id, name, birth, applied, location);
    require(type == "其他微X用户", "用户类型未知");
    return std::make_unique<OtherUserHsx>(id, name, birth, applied, location, shared);
}
}
