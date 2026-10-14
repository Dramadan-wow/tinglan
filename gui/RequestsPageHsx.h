#pragma once
#include "AppSessionHsx.h"
#include <QWidget>
class QTabWidget; class QTableWidget;
namespace im {
class RequestsPageHsx final : public QWidget {
public:
    RequestsPageHsx(AppSessionHsx&,QWidget* parent=nullptr);
    void refresh();
private:
    void respond(bool accept);
    void cancel();
    AppSessionHsx& session_;
    QTabWidget* tabs_;
    QTableWidget *received_,*sent_,*groups_;
};
}
