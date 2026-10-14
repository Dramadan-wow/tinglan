#include "FormDialogHsx.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <stdexcept>
namespace im {
FormDialogHsx::FormDialogHsx(const QString& title, QWidget* parent) : QDialog(parent) {
    setWindowTitle("听澜 · "+title); setMinimumWidth(480); setModal(true);
    auto layout = new QVBoxLayout(this); layout->setContentsMargins(30, 28, 30, 26); layout->setSpacing(24);
    auto* heading=new QLabel(title);heading->setObjectName("sectionTitle");layout->addWidget(heading);
    form_ = new QFormLayout; form_->setSpacing(18);form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow); layout->addLayout(form_);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("确定"); buttons->button(QDialogButtonBox::Ok)->setProperty("primary", true);
    buttons->button(QDialogButtonBox::Cancel)->setText("取消"); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {try{if (!action_ || action_(*this)) accept();}catch(const std::exception& e){QMessageBox::warning(this,"请检查输入",QString::fromUtf8(e.what()));}});
}
void FormDialogHsx::text(const QString& key, const QString& label, const QString& value, bool password) {
    auto input = new QLineEdit(value); input->setObjectName(key); if (password) input->setEchoMode(QLineEdit::Password);
    fields_[key] = input; form_->addRow(label, input);
}
void FormDialogHsx::number(const QString& key, const QString& label, int min, int max, int value) {
    auto input = new QSpinBox; input->setObjectName(key); input->setRange(min, max); input->setValue(value);
    fields_[key] = input; form_->addRow(label, input);
}
void FormDialogHsx::combo(const QString& key, const QString& label, const QList<QPair<QString, QVariant>>& options, std::optional<int> selected) {
    auto input = new QComboBox; input->setObjectName(key); for (const auto& option : options) input->addItem(option.first, option.second);
    if(selected){const int matching=input->findData(*selected);if(matching>=0)input->setCurrentIndex(matching);} fields_[key] = input; form_->addRow(label, input);
}
void FormDialogHsx::check(const QString& key, const QString& label, bool value) {
    auto input = new QCheckBox(label); input->setObjectName(key); input->setChecked(value); fields_[key] = input; form_->addRow("", input);
}
QString FormDialogHsx::textValue(const QString& key) const { return qobject_cast<QLineEdit*>(fields_.value(key))->text(); }
int FormDialogHsx::numberValue(const QString& key) const { return qobject_cast<QSpinBox*>(fields_.value(key))->value(); }
int FormDialogHsx::choice(const QString& key) const { return qobject_cast<QComboBox*>(fields_.value(key))->currentData().toInt(); }
bool FormDialogHsx::checked(const QString& key) const { return qobject_cast<QCheckBox*>(fields_.value(key))->isChecked(); }
void FormDialogHsx::submit(std::function<bool(const FormDialogHsx&)> action) { action_ = std::move(action); }
}
