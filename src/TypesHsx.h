#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace im {
enum class ServiceHsx { QQ, WeChat, Weibo, WeiShang, WeiChang, WeiZou, WeiXiao, Count };
// Keep legacy numeric values readable in old backups; only these three services are offered.
inline constexpr int ActiveServiceCountHsx = 3;
inline bool activeService(ServiceHsx type) { return int(type) >= 0 && int(type) < ActiveServiceCountHsx; }
enum class GroupModeHsx { QQ, WeChat };
enum class MessageKindHsx { Private, Group };
enum class RequestStatusHsx { Pending, Accepted, Rejected, Cancelled };
std::string serviceName(ServiceHsx type);
ServiceHsx serviceType(int value);
ServiceHsx serviceType(const std::string& name);
std::string modeName(GroupModeHsx mode);
void require(bool condition, const std::string& message);
void checkText(const std::string& value, const std::string& field, bool empty = false, size_t limit = 300);
std::string trim(std::string text);
std::int64_t now();
std::string timeText(std::int64_t timestamp);
std::vector<std::string> split(const std::string& text, char delimiter);
}
