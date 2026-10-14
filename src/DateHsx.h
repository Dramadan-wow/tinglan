#pragma once
#include <string>

namespace im {
class DateHsx {
public:
    explicit DateHsx(const std::string& text);
    static DateHsx today();
    std::string str() const;
    int yearsUntil(const DateHsx& other) const;
    bool operator<(const DateHsx& other) const;
private:
    int year_, month_, day_;
};
}
