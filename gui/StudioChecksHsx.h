#pragma once
#include <QString>
#include <QJsonArray>
namespace im {
class StudioChecksHsx final {
public:
    static void run(const QString& directory,QJsonArray& passed);
};
}
