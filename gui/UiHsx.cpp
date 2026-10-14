#include "UiHsx.h"
#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
namespace im {
QTableWidget* UiHsx::table(const QStringList& headers){
    auto* t=new QTableWidget(0,headers.size());t->setHorizontalHeaderLabels(headers);t->setSelectionBehavior(QAbstractItemView::SelectRows);t->setSelectionMode(QAbstractItemView::SingleSelection);t->setEditTriggers(QAbstractItemView::NoEditTriggers);t->setAlternatingRowColors(true);t->setShowGrid(false);t->verticalHeader()->hide();t->verticalHeader()->setDefaultSectionSize(54);t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);return t;
}
void UiHsx::row(QTableWidget* table,const QStringList& values,qlonglong id,int extra){const int row=table->rowCount();table->insertRow(row);for(int col=0;col<values.size();++col){auto* item=new QTableWidgetItem(values[col]);item->setData(Qt::UserRole,id);item->setData(Qt::UserRole+1,extra);table->setItem(row,col,item);}}
qlonglong UiHsx::selected(QTableWidget* t){return t->currentRow()<0?0:t->item(t->currentRow(),0)->data(Qt::UserRole).toLongLong();}
int UiHsx::extra(QTableWidget* t){return t->currentRow()<0?0:t->item(t->currentRow(),0)->data(Qt::UserRole+1).toInt();}
void UiHsx::select(QTableWidget* t,qlonglong id){for(int i=0;i<t->rowCount();++i)if(t->item(i,0)->data(Qt::UserRole).toLongLong()==id){t->selectRow(i);return;}}
QPushButton* UiHsx::button(QHBoxLayout* layout,const QString& text,const std::function<void()>& action,bool primary){auto* b=new QPushButton(text);b->setProperty("primary",primary);layout->addWidget(b);QObject::connect(b,&QPushButton::clicked,b,action);return b;}
QList<QPair<QString,QVariant>> UiHsx::services(const UserHsx& u){QList<QPair<QString,QVariant>> choices;for(const auto& [type,a]:u.services())if(activeService(type))choices.append({AppSessionHsx::q(serviceName(type)),static_cast<int>(type)});return choices;}
QList<QPair<QString,QVariant>> UiHsx::allServices(){QList<QPair<QString,QVariant>> choices;for(int i=0;i<ActiveServiceCountHsx;++i)choices.append({AppSessionHsx::q(serviceName(static_cast<ServiceHsx>(i))),i});return choices;}
bool UiHsx::confirm(QWidget* parent,const QString& message){return QMessageBox::question(parent,"确认操作",message,QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes;}
QList<QPair<QString,QVariant>> UiHsx::users(const DataStoreHsx& data,int exclude){QList<QPair<QString,QVariant>> choices;for(const auto& [id,user]:data.users())if(id!=exclude)choices.append({AppSessionHsx::q(user->name()),id});return choices;}
}
