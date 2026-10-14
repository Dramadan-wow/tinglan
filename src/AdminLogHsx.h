#pragma once
#include "TypesHsx.h"
namespace im {
class AdminLogHsx final {
public:
    AdminLogHsx(long long id, std::int64_t created, std::string actor, std::string action,
        int target, ServiceHsx service);
    long long id() const { return id_; }
    std::int64_t created() const { return created_; }
    const std::string& actor() const { return actor_; }
    const std::string& action() const { return action_; }
    int target() const { return target_; }
    ServiceHsx service() const { return service_; }
private:
    long long id_;
    std::int64_t created_;
    std::string actor_, action_;
    int target_;
    ServiceHsx service_;
};
}
