#include "OtherUserHsx.h"
namespace im {
OtherUserHsx::OtherUserHsx(int id, const std::string& name, DateHsx birth, DateHsx applied,
    const std::string& location, bool shared) : UserHsx(id, name, birth, applied, location), shared_(shared) {}
std::string OtherUserHsx::typeName() const { return "其他微X用户"; }
bool OtherUserHsx::sharesQQ() const { return shared_; }
}
