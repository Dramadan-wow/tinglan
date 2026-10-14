#pragma once
#include "AppSessionHsx.h"
#include <QWidget>
#include <array>
class QLabel; class QTableWidget;
namespace im {
class DataPageHsx final : public QWidget {
public:
    DataPageHsx(AppSessionHsx&,QWidget* parent=nullptr);
    void refresh();
private:
    AppSessionHsx& session_;
    std::array<QLabel*,4> counts_;
    QLabel* state_;
    QTableWidget* services_;
};
}
