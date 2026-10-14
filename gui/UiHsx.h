#pragma once
#include "AppSessionHsx.h"
#include <QList>
#include <QPair>
#include <QVariant>
#include <QStringList>
class QTableWidget; class QHBoxLayout; class QPushButton; class QWidget;
namespace im {
class UiHsx final {
public:
    static QTableWidget* table(const QStringList& headers);
    static void row(QTableWidget* table, const QStringList& values, qlonglong id, int extra = 0);
    static qlonglong selected(QTableWidget* table);
    static int extra(QTableWidget* table);
    static void select(QTableWidget* table, qlonglong id);
    static QPushButton* button(QHBoxLayout* layout,const QString& text,const std::function<void()>& action,bool primary=false);
    static QList<QPair<QString,QVariant>> services(const UserHsx& user);
    static QList<QPair<QString,QVariant>> allServices();
    static QList<QPair<QString,QVariant>> users(const DataStoreHsx& data, int exclude = 0);
    static bool confirm(QWidget* parent,const QString& message);
};
}
