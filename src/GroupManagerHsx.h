#pragma once
#include "DataStoreHsx.h"
#include <vector>
namespace im {
class GroupManagerHsx {
public:
    explicit GroupManagerHsx(DataStoreHsx& data) : data_(data) {}
    int create(int actor, ServiceHsx type, const std::string& name, GroupModeHsx mode, int parent = 0);
    void apply(int actor, ServiceHsx type, int group);
    void invite(int actor, ServiceHsx type, int group, int target, ServiceHsx destination);
    void approve(int actor, ServiceHsx type, int group, int target, ServiceHsx destination, bool accept);
    void cancelApplication(int actor, ServiceHsx type, int group);
    void leave(int actor, ServiceHsx type, int group);
    void dropService(int actor, ServiceHsx type);
    void kick(int actor, ServiceHsx type, int group, int target);
    void admin(int actor, ServiceHsx type, int group, int target, bool enabled);
    void transfer(int actor, ServiceHsx type, int group, int target);
    void edit(int actor, ServiceHsx type, int group, const std::string& name, const std::string& announcement, int capacity);
    void mode(int actor, ServiceHsx type, int group, GroupModeHsx mode);
    void mute(int actor, ServiceHsx type, int group, int target, int minutes);
    void disband(int actor, ServiceHsx type, int group);
    void requireMember(int actor, ServiceHsx type, int group) const;
private:
    void requireManager(int actor, ServiceHsx type, int group, bool owner = false) const;
    void checkJoin(int group, int target, ServiceHsx type) const;
    void checkLeave(const GroupHsx& group, int user, const std::set<ServiceHsx>& services) const;
    void drop(GroupHsx& group, int user, const std::set<ServiceHsx>& services);
    std::vector<int> family(int group) const;
    DataStoreHsx& data_;
};
}
