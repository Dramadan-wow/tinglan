#pragma once
#include <functional>
namespace im {
class ScopeHsx {
public:
    explicit ScopeHsx(std::function<void()> cleanup);
    ~ScopeHsx();
    ScopeHsx(const ScopeHsx&) = delete;
    ScopeHsx& operator=(const ScopeHsx&) = delete;
private:
    std::function<void()> cleanup_;
};
}
