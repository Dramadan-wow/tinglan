#pragma once
#include "DataStoreHsx.h"
namespace im {
class AccountManagerHsx {
public:
    explicit AccountManagerHsx(DataStoreHsx& data) : data_(data) {}
    void registerUser(int id, const std::string& name, const std::string& birth, const std::string& location,
        ServiceHsx primary, long long number, const std::string& password, bool shared = false,
        const std::string& applied = DateHsx::today().str());
    void login(int id, ServiceHsx type, const std::string& password);
    int findByNickname(const std::string& nickname) const;
    int availableUserId() const;
    void logout(int id);
    void profile(int id, const std::string& name, const std::string& location);
    void open(int id, ServiceHsx type, long long number, const std::string& nick, const std::string& password, bool shared = false);
    void close(int id, ServiceHsx type);
    void edit(int id, ServiceHsx type, const std::string& nick, const std::string& currentPassword, const std::string& newPassword);
    void bind(int id, long long qqNumber);
private:
    void uniqueNumber(int owner, ServiceHsx type, long long number) const;
    void uniqueName(int owner, const std::string& nickname) const;
    DataStoreHsx& data_;
};
}
