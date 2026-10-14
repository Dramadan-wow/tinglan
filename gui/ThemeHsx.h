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
};
}
