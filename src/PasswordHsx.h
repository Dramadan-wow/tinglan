#pragma once
#include <string>
namespace im {
class PasswordHsx final {
public:
    static std::string hash(const std::string& password, size_t minimum = 8, const std::string& field = "密码");
    static bool verify(const std::string& password, const std::string& encoded);
    static bool valid(const std::string& encoded);
};
}
