#include "MessageHsx.h"
#include <utility>
namespace im {
MessageHsx::MessageHsx(long long id, MessageKindHsx kind, int sender, int target,
    ServiceHsx source, ServiceHsx destination, std::string text, std::int64_t created)
    : id_(id), kind_(kind), sender_(sender), target_(target), source_(source), destination_(destination),
      text_(std::move(text)), created_(created) {
    require(id > 0 && sender > 0 && target > 0 && created > 0, "消息字段无效");
    require(kind == MessageKindHsx::Private || kind == MessageKindHsx::Group, "消息类型无效");
    serviceType(static_cast<int>(source)); serviceType(static_cast<int>(destination));
    checkText(text_, "消息", true, 6000);
}
}
