#pragma once
#include "TypesHsx.h"
#include <set>
namespace im {
class MessageHsx {
public:
    MessageHsx(long long id, MessageKindHsx kind, int sender, int target, ServiceHsx source,
        ServiceHsx destination, std::string text, std::int64_t created);
    long long id() const { return id_; }
    MessageKindHsx kind() const { return kind_; }
    int sender() const { return sender_; }
    int target() const { return target_; }
    ServiceHsx source() const { return source_; }
    ServiceHsx destination() const { return destination_; }
    const std::string& text() const { return text_; }
    std::int64_t created() const { return created_; }
    bool recalled() const { return recalled_; }
    bool read(int user) const { return readBy_.count(user) != 0; }
    bool starred(int user) const { return stars_.count(user) != 0; }
private:
    friend class WorkspaceCodecHsx;
    friend class WorkspaceBackendHsx;
    friend class MessageManagerHsx;
    friend class StorageHsx;
    long long id_;
    MessageKindHsx kind_;
    int sender_, target_;
    ServiceHsx source_, destination_;
    std::string text_;
    std::int64_t created_;
    bool recalled_ = false;
    std::set<int> readBy_, stars_;
};
}
