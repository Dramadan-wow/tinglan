#pragma once
#include <string>
namespace im {
class PasswordHsx final {
public:
    // Fresh credentials need eight bytes; hashes restored from an older archive are
    // re-hashed with a minimum of one so existing short passwords keep signing in.
    static constexpr size_t minimumLength = 8;
    static void check(const std::string& password, size_t minimum = minimumLength, const std::string& field = "密码");
    static std::string hash(const std::string& password, size_t minimum = minimumLength, const std::string& field = "密码");
    static bool verify(const std::string& password, const std::string& encoded);
    static bool valid(const std::string& encoded);
};
}
