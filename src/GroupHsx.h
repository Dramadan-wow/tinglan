#pragma once
#include "GroupPolicyHsx.h"
#include <map>
#include <set>
namespace im {
class GroupHsx {
public:
    GroupHsx(int id, std::string name, int owner, GroupModeHsx native, int parent = 0);
    virtual ~GroupHsx() = default;
    virtual std::string typeName() const = 0;
    int id() const { return id_; }
    int owner() const { return owner_; }
    int parent() const { return parent_; }
    int capacity() const { return capacity_; }
    GroupModeHsx native() const { return native_; }
    const std::string& name() const { return name_; }
    const std::string& announcement() const { return announcement_; }
    const GroupPolicyHsx& policy() const { return *policy_; }
    const std::map<int, std::set<ServiceHsx>>& members() const { return members_; }
    const std::set<int>& admins() const { return admins_; }
    const std::set<std::pair<int, ServiceHsx>>& pending() const { return pending_; }
    bool contains(int user, ServiceHsx service) const;
    bool contains(int user) const { return members_.count(user) != 0; }
    bool manages(int user) const;
    bool muted(int user) const;
    static std::unique_ptr<GroupHsx> create(int id, const std::string& name, int owner, GroupModeHsx native, int parent = 0);
private:
    friend class WorkspaceCodecHsx;
    friend class WorkspaceBackendHsx;
    friend class GroupManagerHsx;
    friend class StorageHsx;
    int id_, owner_, parent_, capacity_ = 500;
    GroupModeHsx native_;
    std::string name_, announcement_;
    std::unique_ptr<GroupPolicyHsx> policy_;
    std::map<int, std::set<ServiceHsx>> members_;
    std::set<int> admins_;
    std::set<std::pair<int, ServiceHsx>> pending_;
    std::map<int, std::int64_t> mutedUntil_;
};
}
