#include "DateHsx.h"
#include "TypesHsx.h"
#include <tuple>

namespace im {
DateHsx::DateHsx(const std::string& text) {
    require(text.size() == 10 && text[4] == '-' && text[7] == '-', "日期格式为 yyyy-mm-dd");
    for (size_t i = 0; i < text.size(); ++i)
        if (i != 4 && i != 7) require(text[i] >= '0' && text[i] <= '9', "日期含有非数字");
    year_ = std::stoi(text.substr(0, 4)); month_ = std::stoi(text.substr(5, 2)); day_ = std::stoi(text.substr(8, 2));
    const bool leap = year_ % 400 == 0 || (year_ % 4 == 0 && year_ % 100 != 0);
    const int days[] = {31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    require(year_ >= 1900 && year_ <= 9999 && month_ >= 1 && month_ <= 12 &&
        day_ >= 1 && day_ <= days[month_ - 1], "日期不存在");
}
DateHsx DateHsx::today() { return DateHsx(timeText(now()).substr(0, 10)); }
std::string DateHsx::str() const {
    return std::to_string(year_) + '-' + (month_ < 10 ? "0" : "") + std::to_string(month_) +
        '-' + (day_ < 10 ? "0" : "") + std::to_string(day_);
}
int DateHsx::yearsUntil(const DateHsx& other) const {
    const int age = other.year_ - year_ - (std::tie(other.month_, other.day_) < std::tie(month_, day_));
    return age > 0 ? age : 0;
}
bool DateHsx::operator<(const DateHsx& other) const {
    return std::tie(year_, month_, day_) < std::tie(other.year_, other.month_, other.day_);
}
}
