#pragma once
#include "UserHsx.h"
#include "GroupHsx.h"
#include "MessageHsx.h"
#include "FriendRequestHsx.h"
#include "AdminLogHsx.h"

namespace im {
// 唯一所有者：跨对象关系保存ID，业务模块不持有裸指针。
class DataStoreHsx {
public:
    const UserHsx& user(int id) const;
    const ServiceAccountHsx& account(int user, ServiceHsx type) const;
    const GroupHsx& group(int id) const;
    const MessageHsx& message(long long id) const;
    void requireOnline(int user) const;
    const auto& users() const { return users_; }
    const auto& groups() const { return groups_; }
    const auto& messages() const { return messages_; }
    const auto& requests() const { return requests_; }
    const auto& adminLogs() const { return adminLogs_; }
    bool dirty() const { return dirty_; }
private:
    friend class WorkspaceCodecHsx;
    friend class WorkspaceBackendHsx;
    friend class AccountManagerHsx;
    friend class AdminManagerHsx;
    friend class FriendManagerHsx;
    friend class GroupManagerHsx;
    friend class MessageManagerHsx;
    friend class StorageHsx;
    UserHsx& editUser(int id);
    ServiceAccountHsx& editAccount(int user, ServiceHsx type);
    GroupHsx& editGroup(int id);
    void audit(const std::string& actor, const std::string& action, int target = 0, ServiceHsx service = ServiceHsx::Count);
    std::map<int, std::unique_ptr<UserHsx>> users_;
    std::map<int, std::unique_ptr<GroupHsx>> groups_;
    std::map<long long, MessageHsx> messages_;
    std::map<long long, FriendRequestHsx> requests_;
    std::map<long long, AdminLogHsx> adminLogs_;
    std::string adminName_ = "admin", adminHash_;
    bool adminOnline_ = false;
    long long nextAudit_ = 1;
    int nextGroup_ = 1001;
    long long nextMessage_ = 1, nextRequest_ = 1;
    bool dirty_ = false;
};
}
