#pragma once
#include "GroupHsx.h"
namespace im {
class QQGroupHsx final : public GroupHsx {
public:
    QQGroupHsx(int id, const std::string& name, int owner, int parent = 0);
    std::string typeName() const override;
};
}
