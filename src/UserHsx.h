#pragma once
#include "DateHsx.h"
#include "ServiceAccountHsx.h"
#include <memory>

namespace im {
// 用户类族描述账号来源；已开通服务通过组合维护，不复制好友和群数据。
class UserHsx {
public:
    UserHsx(int id, std::string name, DateHsx birth, DateHsx applied, std::string location);
    virtual ~UserHsx() = default;
    virtual std::string typeName() const = 0;
    virtual bool sharesQQ() const = 0;
    int id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& location() const { return location_; }
    const DateHsx& birth() const { return birth_; }
    const DateHsx& applied() const { return applied_; }
    int tAge() const { return applied_.yearsUntil(DateHsx::today()); }
    bool online() const { return online_; }
    bool loginAllowed() const { return !disabled_; }
    const std::map<ServiceHsx, ServiceAccountHsx>& services() const { return services_; }
    static std::unique_ptr<UserHsx> create(const std::string& type, int id, const std::string& name,
        const DateHsx& birth, const DateHsx& applied, const std::string& location, bool shared = false);
private:
    friend class WorkspaceCodecHsx;
    friend class WorkspaceBackendHsx;
    friend class DataStoreHsx;
    friend class AccountManagerHsx;
    friend class AdminManagerHsx;
    friend class StorageHsx;
    int id_;
    std::string name_, location_;
    DateHsx birth_, applied_;
    bool online_ = false, disabled_ = false;
    std::map<ServiceHsx, ServiceAccountHsx> services_;
};
}
