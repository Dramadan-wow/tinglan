#pragma once
#include "AccountManagerHsx.h"
#include "FriendManagerHsx.h"
#include "GroupManagerHsx.h"
#include "MessageManagerHsx.h"
#include <functional>
#include <filesystem>

namespace im {
// 输入、导航、保存统一实现；每个功能界面只组织自己的操作。
class ConsoleHsx {
public:
    using Action = std::pair<std::string, std::function<void()>>;
    ConsoleHsx(DataStoreHsx& data, std::filesystem::path file);
    void run();
    void menu(const std::string& title, const std::vector<Action>& actions, const std::function<bool()>& guard = {});
    std::string text(const std::string& prompt);
    std::string password(const std::string& prompt);
    long long number(const std::string& prompt);
    int id(const std::string& prompt);
    bool confirm(const std::string& prompt);
    ServiceHsx chooseService(int user = 0);
    GroupModeHsx chooseMode();
    void requireSession() const;
    void session(int user, ServiceHsx type);
    void showUser(int id) const;
    void showUsers(const std::vector<int>& users) const;
    void showMessages(const std::vector<const MessageHsx*>& messages, bool markRead = true);
    void save();
    int user() const { return user_; }
    ServiceHsx service() const { return service_; }
    DataStoreHsx& data() { return data_; }
    AccountManagerHsx& accounts() { return accounts_; }
    FriendManagerHsx& friends() { return friends_; }
    GroupManagerHsx& groups() { return groups_; }
    MessageManagerHsx& messages() { return messages_; }
    const std::filesystem::path& file() const { return file_; }
private:
    void status() const;
    void tools();
    DataStoreHsx& data_;
    std::filesystem::path file_;
    AccountManagerHsx accounts_;
    FriendManagerHsx friends_;
    GroupManagerHsx groups_;
    MessageManagerHsx messages_;
    int user_ = 0;
    ServiceHsx service_ = ServiceHsx::Count;
};
}
