#pragma once
#include <QString>
class QApplication;
class QLabel;
class QWidget;
namespace im {
class ThemeHsx {
public:
    static void install(QApplication& app, const QString& theme = "jade");
    static QLabel* avatar(const QString& name, int seed, int size = 42, QWidget* parent = nullptr);
    static QString color(int seed);
    static QString workspace(const QString& theme);
    // Signed-in workspace accent for a theme id; jade and ocean share the workspace blue.
    static QString accent(const QString& theme);
};
}
