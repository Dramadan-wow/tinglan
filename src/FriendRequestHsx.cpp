#include "FriendRequestHsx.h"
#include <utility>
namespace im {
FriendRequestHsx::FriendRequestHsx(long long id, int from, int to, ServiceHsx source,
    ServiceHsx destination, std::string reason)
    : id_(id), from_(from), to_(to), source_(source), destination_(destination), reason_(std::move(reason)) {
    require(id > 0 && from > 0 && to > 0 && from != to, "好友申请编号无效");
    serviceType(static_cast<int>(source)); serviceType(static_cast<int>(destination)); checkText(reason_, "验证信息", true);
}
}
