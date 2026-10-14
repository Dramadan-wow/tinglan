#pragma once
#include "DataStoreHsx.h"
#include <QJsonObject>
namespace im {
// Shared serialization adapter. viewer=0 is a server-only, credential-bearing backup.
class WorkspaceCodecHsx {
public:
    static QJsonObject encode(const DataStoreHsx&,int viewer=0);
    static DataStoreHsx decode(const QJsonObject&);
};
}

