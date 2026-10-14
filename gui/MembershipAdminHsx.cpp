#include "MembershipAdminHsx.h"
#include "FormDialogHsx.h"
#include "UiHsx.h"
#include <QComboBox>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
namespace im {
MembershipAdminHsx::MembershipAdminHsx(AppSessionHsx& s,QWidget* parent):QWidget(parent),session_(s){
    auto* l=new QVBoxLayout(this);auto* hint=new QLabel("请到微信收款记录核对金额、付款人和时间。此处的提交记录不是到账凭证；确认到账后再开通。");hint->setWordWrap(true);hint->setObjectName("muted");l->addWidget(hint);filter_=new QComboBox;filter_->setObjectName("paymentOrderFilter");filter_->addItems({"全部订单","待核验","已开通","已驳回","已取消"});l->addWidget(filter_);table_=UiHsx::table({"订单号","昵称 / 用户 ID","档位","金额","提交时间","状态","核验人"});table_->setObjectName("adminPaymentOrders");l->addWidget(table_,1);details_=new QLabel("选择订单查看付款备注与核验记录");details_->setWordWrap(true);details_->setObjectName("paymentOrderDetails");l->addWidget(details_);auto* actions=new QHBoxLayout;UiHsx::button(actions,"确认到账并开通",[this]{review(true);},true)->setObjectName("approvePaymentOrder");UiHsx::button(actions,"驳回申请",[this]{review(false);})->setObjectName("rejectPaymentOrder");actions->addStretch();l->addLayout(actions);connect(filter_,&QComboBox::currentIndexChanged,this,[this]{refresh();});connect(table_,&QTableWidget::itemSelectionChanged,this,[this]{details();});refresh();
}
void MembershipAdminHsx::refresh(){if(!session_.adminSignedIn())return;QString selected;if(table_->currentRow()>=0)selected=table_->item(table_->currentRow(),0)->data(Qt::UserRole+3).toString();QSignalBlocker guard(table_);table_->setRowCount(0);const QStringList codes{"","pending","approved","rejected","cancelled"};
    for(const auto& value:session_.studio().orders(true)){const auto o=value.toObject();const auto code=o.value("status").toString();if(filter_->currentIndex()&&code!=codes[filter_->currentIndex()])continue;const int user=o.value("user").toInt();const auto id=o.value("id").toString();const auto state=code=="pending"?"待核验":code=="approved"?"已开通":code=="rejected"?"已驳回":"已取消";UiHsx::row(table_,{id.left(8),AppSessionHsx::q(session_.data().user(user).name())+" / "+QString::number(user),StudioStoreHsx::plan(o.value("tier").toInt()).value("name").toString(),QString("¥%1").arg(o.value("price").toInt()),QDateTime::fromSecsSinceEpoch(static_cast<qint64>(o.value("created").toDouble())).toString("MM-dd HH:mm"),state,o.value("reviewer").toString()},user);auto* item=table_->item(table_->rowCount()-1,0);item->setData(Qt::UserRole+3,id);item->setData(Qt::UserRole+4,o);if(id==selected)table_->selectRow(table_->rowCount()-1);}
    if(table_->currentRow()<0&&table_->rowCount())table_->selectRow(0);details();
}
void MembershipAdminHsx::details(){if(table_->currentRow()<0){details_->setText("暂无符合条件的订单");return;}const auto o=table_->item(table_->currentRow(),0)->data(Qt::UserRole+4).toJsonObject();details_->setText("订单："+o.value("id").toString()+"\n付款备注："+o.value("reference").toString()+"\n核验记录："+o.value("note").toString("待处理"));details_->setTextFormat(Qt::PlainText);}
void MembershipAdminHsx::review(bool approve){if(table_->currentRow()<0)return;const auto o=table_->item(table_->currentRow(),0)->data(Qt::UserRole+4).toJsonObject();const auto id=o.value("id").toString();FormDialogHsx form(approve?QString("核验到账 ¥%1 · 开通会员").arg(o.value("price").toInt()):"驳回会员申请",this);form.text("reviewNote",approve?"完整微信交易单号（不可重复）":"驳回原因");if(approve)form.check("paymentVerified","我已核对微信收款记录、付款人及金额");form.submit([&,approve](const FormDialogHsx& f){require(!approve||f.checked("paymentVerified"),"请先核验实际到账，再勾选确认");return session_.command("admin.membership.review",{{"id",id},{"approve",approve},{"verified",approve&&f.checked("paymentVerified")},{"note",f.textValue("reviewNote")}},[&]{session_.studio().decideOrder(id,approve,f.textValue("reviewNote"));},approve?"到账核验已记录，会员权益已开通":"已记录驳回原因");});form.exec();}
}
