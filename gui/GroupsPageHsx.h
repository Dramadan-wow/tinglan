#pragma once
#include "AppSessionHsx.h"
#include <QWidget>
class QTableWidget; class QListWidget; class QLabel; class QLineEdit; class QComboBox;
namespace im {
class GroupsPageHsx final : public QWidget {
    Q_OBJECT
public:
    GroupsPageHsx(AppSessionHsx&,QWidget* parent=nullptr);
    void refresh();
signals:
    void openChat(int group);
private:
    int selected() const;
    void details();
    void create(bool subgroup);
    void invite();
    void edit();
    void memberAction(int action);
    void changeMode();
    AppSessionHsx& session_;
    QListWidget* list_;
    QTableWidget* members_;
    QLabel *title_,*description_;
    QLineEdit* search_;
    QComboBox *filter_,*operations_;
};
}
