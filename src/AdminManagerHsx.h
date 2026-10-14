#pragma once
#include "DataStoreHsx.h"
namespace im {
class AdminManagerHsx final {
public:
    explicit AdminManagerHsx(DataStoreHsx& data) : data_(data) {}
    void initialize();
    void login(const std::string& name, const std::string& password);
    void logout();
    bool active() const { return data_.adminOnline_; }
    const std::string& name() const { return data_.adminName_; }
    void requireAdmin() const;
    void setLoginAllowed(int user, bool allowed);
    void forceLogout(int user);
    void resetPassword(int user, ServiceHsx service, const std::string& password);
    void changePassword(const std::string& original, const std::string& password);
private:
    DataStoreHsx& data_;
};
}
