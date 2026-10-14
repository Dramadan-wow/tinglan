#pragma once
#include <QDialog>
#include <QMap>
#include <QVariant>
#include <functional>
#include <optional>
class QFormLayout;
namespace im {
class FormDialogHsx final : public QDialog {
public:
    explicit FormDialogHsx(const QString& title, QWidget* parent = nullptr);
    void text(const QString& key, const QString& label, const QString& value = {}, bool password = false);
    void number(const QString& key, const QString& label, int min, int max, int value);
    void combo(const QString& key, const QString& label, const QList<QPair<QString, QVariant>>& options, std::optional<int> selected = std::nullopt);
    void check(const QString& key, const QString& label, bool value = false);
    QString textValue(const QString& key) const;
    int numberValue(const QString& key) const;
    int choice(const QString& key) const;
    bool checked(const QString& key) const;
    void submit(std::function<bool(const FormDialogHsx&)> action);
private:
    QMap<QString, QWidget*> fields_;
    QFormLayout* form_;
    std::function<bool(const FormDialogHsx&)> action_;
};
}
