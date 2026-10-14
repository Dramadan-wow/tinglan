#pragma once
#include <QIcon>
#include <QString>
namespace im {
// 所有细线图标由同一组矢量路径绘制，支持高DPI及选中状态。
class IconHsx final {
public:
    static QIcon get(const QString& name, const QString& color = "#6d827b", const QString& selected = "#ffffff");
};
}
