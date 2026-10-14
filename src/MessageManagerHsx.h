#pragma once
#include "DataStoreHsx.h"
#include <filesystem>
#include <vector>
namespace im {
class MessageManagerHsx {
public:
    explicit MessageManagerHsx(DataStoreHsx& data) : data_(data) {}
    long long sendPrivate(int actor, ServiceHsx type, int target, const std::string& text);
    long long sendGroup(int actor, ServiceHsx type, int group, const std::string& text, int mention = 0);
    std::vector<const MessageHsx*> history(int actor, ServiceHsx type, MessageKindHsx kind, int target, const std::string& keyword = "") const;
    std::vector<const MessageHsx*> inbox(int actor, ServiceHsx type, bool onlyUnread = false, const std::string& keyword = "") const;
    std::vector<const MessageHsx*> favorites(int actor, ServiceHsx type) const;
    void markRead(int actor, ServiceHsx type, long long id);
    void recall(int actor, long long id);
    void star(int actor, ServiceHsx type, long long id, bool enabled);
    void exportHistory(int actor, ServiceHsx type, MessageKindHsx kind, int target, const std::filesystem::path& path) const;
    bool visible(int actor, ServiceHsx type, const MessageHsx& message) const;
private:
    long long append(MessageKindHsx kind, int actor, int target, ServiceHsx source, ServiceHsx destination, const std::string& text);
    DataStoreHsx& data_;
};
}
