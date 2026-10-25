#pragma once
#include <string>
namespace im {
class PasswordHsx final {
public:
    // Fresh local (coursework) credentials need eight bytes; hashes restored from an older archive are
    // re-hashed with a minimum of one so existing short passwords keep signing in.
    static constexpr size_t minimumLength = 8;
    // Passwords chosen for a networked server account.
    static constexpr size_t RemoteMinimum = 12;
    static void check(const std::string& password, size_t minimum = minimumLength, const std::string& field = "密码");
    static std::string hash(const std::string& password, size_t minimum = minimumLength, const std::string& field = "密码");
    static bool verify(const std::string& password, const std::string& encoded);
    static bool valid(const std::string& encoded);
    // True for a valid digest made with fewer iterations than hash() now uses; rehash after a successful login.
    static bool outdated(const std::string& encoded);
};
}
