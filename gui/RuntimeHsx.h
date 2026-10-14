#pragma once
#include <QString>
namespace im {
struct RuntimeHsx {QString data,config;};
QString runtimeDirectory();
RuntimeHsx prepareRuntime(const QString& configOverride = {});
}
