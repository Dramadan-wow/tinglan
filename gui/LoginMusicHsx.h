#pragma once
#include <QWidget>
class QMediaPlayer;class QAudioOutput;class QLabel;class QPushButton;class QSlider;
namespace im {
class LoginMusicHsx final : public QWidget {
public:
    explicit LoginMusicHsx(QWidget* parent=nullptr);
    void stop();
private:
    void selectMusic();
    void refresh();
    QMediaPlayer* player_;
    QAudioOutput* audio_;
    QPushButton* play_;
    QLabel *title_,*status_;
    QSlider* progress_;
};
}
