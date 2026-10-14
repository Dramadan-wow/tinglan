#pragma once
#include "DataStoreHsx.h"
#include <vector>
namespace im {
class FriendManagerHsx {
public:
    explicit FriendManagerHsx(DataStoreHsx& data) : data_(data) {}
    long long request(int actor, ServiceHsx source, int target, ServiceHsx destination, const std::string& reason);
    void respond(int actor, ServiceHsx type, long long requestId, bool accept);
    void cancel(int actor, long long requestId);
    void edit(int actor, ServiceHsx type, int target, const std::string& remark, const std::string& category);
    void remove(int actor, ServiceHsx type, int target);
    void block(int actor, ServiceHsx type, int target, bool enabled);
    std::vector<int> common(int actor, ServiceHsx first, ServiceHsx second) const;
    std::vector<int> recommend(int actor, ServiceHsx destination, ServiceHsx source) const;
    bool allowed(int actor, ServiceHsx source, int target, ServiceHsx destination) const;
private:
    DataStoreHsx& data_;
};
}
