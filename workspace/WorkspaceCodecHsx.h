#pragma once
#include "DataStoreHsx.h"
#include <QJsonObject>
#include <set>
namespace im {
// Shared serialization adapter. viewer=0 is a server-only, credential-bearing backup.
// viewer>0 is one user's view: birth/registration dates stay private, location and presence
// are limited to friends, service numbers to friends, and people the viewer cannot meet are left out.
class WorkspaceCodecHsx {
public:
    // viewer>0 trims the snapshot to that user; met adds people they also meet elsewhere (public Weibo).
    static QJsonObject encode(const DataStoreHsx&,int viewer=0,const std::set<int>& met={});
    static DataStoreHsx decode(const QJsonObject&);
};
}

