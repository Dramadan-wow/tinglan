#pragma once
#include <string>
namespace im {
class PasswordHsx final {
public:
    // Passwords chosen for a networked server account. Local course data keeps the shorter default.
    static constexpr size_t RemoteMinimum = 12;
    static std::string hash(const std::string& password, size_t minimum = 8);
    static bool verify(const std::string& password, const std::string& encoded);
    static bool valid(const std::string& encoded);
    // True for a valid digest made with fewer iterations than hash() now uses; rehash after a successful login.
    static bool outdated(const std::string& encoded);
};
}
