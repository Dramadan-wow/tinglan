#include "FriendHsx.h"
#include <utility>
namespace im {
FriendHsx::FriendHsx(int user, ServiceHsx contact, std::string remark, std::string category)
    : user_(user), contact_(contact), remark_(std::move(remark)), category_(std::move(category)) {
    require(user > 0, "好友ID无效"); serviceType(static_cast<int>(contact));
    checkText(remark_, "备注", true); checkText(category_, "分组");
}
}
