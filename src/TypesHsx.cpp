#include "TypesHsx.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace im {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
ServiceHsx serviceType(int value) {
    require(value >= 0 && value < static_cast<int>(ServiceHsx::Count), "服务类型无效");
    return static_cast<ServiceHsx>(value);
}
std::string serviceName(ServiceHsx type) {
    static const char* names[] = {"QQ", "微信", "微博", "微商", "微唱", "微走", "微笑"};
    return names[static_cast<int>(serviceType(static_cast<int>(type)))];
}
ServiceHsx serviceType(const std::string& name) {
    static const char* old[] = {"QQ", "WeChat", "Weibo", "WeiShang", "WeiChang", "WeiZou", "WeiXiao"};
    for (int i = 0; i < static_cast<int>(ServiceHsx::Count); ++i)
        if (name == old[i] || name == serviceName(serviceType(i))) return serviceType(i);
    throw std::runtime_error("未知服务：" + name);
}
std::string modeName(GroupModeHsx mode) {
    require(mode == GroupModeHsx::QQ || mode == GroupModeHsx::WeChat, "群模式无效");
    return mode == GroupModeHsx::QQ ? "QQ群" : "微信群";
}
std::string trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}
void checkText(const std::string& value, const std::string& field, bool empty, size_t limit) {
    require(empty || !trim(value).empty(), field + "不能为空");
    require(value.size() <= limit, field + "过长");
    for (unsigned char c : value) require(c >= 32 && c != 127, field + "含有控制字符");
}
std::int64_t now() { return static_cast<std::int64_t>(std::time(nullptr)); }
std::string timeText(std::int64_t timestamp) {
    const auto value = static_cast<std::time_t>(timestamp);
    std::tm local{};
#ifdef _WIN32
    require(localtime_s(&local, &value) == 0, "时间无效");
#else
    require(localtime_r(&value, &local) != nullptr, "时间无效");
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}
std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> result;
    size_t begin = 0;
    while (true) {
        const auto end = text.find(delimiter, begin);
        result.push_back(text.substr(begin, end == std::string::npos ? end : end - begin));
        if (end == std::string::npos) return result;
        begin = end + 1;
    }
}
}
