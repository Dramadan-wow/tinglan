#pragma once
#include "TypesHsx.h"
namespace im {
class FriendRequestHsx {
public:
    FriendRequestHsx(long long id, int from, int to, ServiceHsx source, ServiceHsx destination, std::string reason);
    long long id() const { return id_; }
    int from() const { return from_; }
    int to() const { return to_; }
    ServiceHsx source() const { return source_; }
    ServiceHsx destination() const { return destination_; }
    RequestStatusHsx status() const { return status_; }
    const std::string& reason() const { return reason_; }
private:
    friend class WorkspaceCodecHsx;
    friend class WorkspaceBackendHsx;
    friend class FriendManagerHsx;
    friend class AccountManagerHsx;
    friend class StorageHsx;
    long long id_;
    int from_, to_;
    ServiceHsx source_, destination_;
    std::string reason_;
    RequestStatusHsx status_ = RequestStatusHsx::Pending;
};
}
