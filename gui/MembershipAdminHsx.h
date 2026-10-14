#pragma once
#include "AppSessionHsx.h"
#include <QWidget>
class QTableWidget;class QComboBox;class QLabel;
namespace im {
class MembershipAdminHsx final : public QWidget {
public:
    explicit MembershipAdminHsx(AppSessionHsx&,QWidget* parent=nullptr);
    void refresh();
private:
    void review(bool approve);
    void details();
    AppSessionHsx& session_;
    QTableWidget* table_;
    QComboBox* filter_;
    QLabel* details_;
};
}
