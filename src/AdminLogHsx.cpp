#include "AdminLogHsx.h"
#include <utility>
namespace im {
AdminLogHsx::AdminLogHsx(long long id, std::int64_t created, std::string actor, std::string action, int target, ServiceHsx service)
    : id_(id), created_(created), actor_(std::move(actor)), action_(std::move(action)), target_(target), service_(service) {
    require(id > 0 && created > 0 && target >= 0, "管理日志字段无效");
    checkText(actor_, "日志操作人", false, 128); checkText(action_, "日志操作", false, 128);
    if (service != ServiceHsx::Count) serviceType(static_cast<int>(service));
}
}
