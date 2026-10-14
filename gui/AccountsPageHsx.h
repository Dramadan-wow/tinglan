#pragma once
#include "AppSessionHsx.h"
#include <QWidget>
class QTableWidget; class QLabel;
namespace im {
class AccountsPageHsx final : public QWidget {
public:
    AccountsPageHsx(AppSessionHsx&,QWidget* parent=nullptr);
    void refresh();
private:
    void profile();
    void open();
    void edit();
    void closeService();
    void bind();
    AppSessionHsx& session_;
    QTableWidget* table_;
    QLabel *name_,*details_;
};
}
