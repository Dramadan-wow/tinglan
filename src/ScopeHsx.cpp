#include "ScopeHsx.h"
#include <utility>
namespace im {
ScopeHsx::ScopeHsx(std::function<void()> cleanup) : cleanup_(std::move(cleanup)) {}
ScopeHsx::~ScopeHsx() { cleanup_(); }
}
