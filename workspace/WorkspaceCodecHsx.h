#pragma once
#include "DataStoreHsx.h"
#include <QJsonObject>
namespace im {
// Shared serialization adapter. viewer=0 is a server-only, credential-bearing backup.
// viewer>0 is one user's view: birth/registration dates stay private, location and presence
// are limited to friends, and other users keep only public identity (name, service numbers).
class WorkspaceCodecHsx {
public:
    static QJsonObject encode(const DataStoreHsx&,int viewer=0);
    static DataStoreHsx decode(const QJsonObject&);
};
}

