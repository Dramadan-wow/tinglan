#include "LoginMusicHsx.h"
#include "IconHsx.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QUrl>
#include <QVBoxLayout>
namespace im {
LoginMusicHsx::LoginMusicHsx(QWidget* parent):QWidget(parent) {
    setObjectName("loginMusic");setAttribute(Qt::WA_StyledBackground,true);
    setStyleSheet(R"(
        QWidget#loginMusic {background:rgba(24,54,44,225);border:1px solid rgba(234,242,216,48);border-radius:17px;}
        QWidget#loginMusic QLabel {color:#eef0da;background:transparent;}
        QWidget#loginMusic QPushButton {background:transparent;border:none;border-radius:16px;padding:4px 7px;color:#b7c9aa;font-size:10px;}
        QWidget#loginMusic QPushButton:hover {background:rgba(220,235,196,25);color:#f0f5df;}
        QWidget#loginMusic QPushButton#musicPlay {background:#e4edce;color:#31523b;border:none;border-radius:12px;padding:8px 14px;font-size:11px;font-weight:600;}
        QWidget#loginMusic QPushButton#musicPlay:hover {background:#f4f8e8;}
        QSlider::groove:horizontal {background:rgba(202,225,183,40);height:3px;border-radius:1px;}
        QSlider::sub-page:horizontal {background:#afc39c;border-radius:1px;}
        QSlider::handle:horizontal {background:#dae7c3;width:8px;height:8px;margin:-3px 0;border-radius:4px;}
    )");
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(20,17,20,14);layout->setSpacing(8);auto* head=new QHBoxLayout;head->setSpacing(12);
    auto* disc=new QLabel;disc->setFixedSize(40,40);disc->setAlignment(Qt::AlignCenter);disc->setPixmap(IconHsx::get("music","#d6e3c0").pixmap(23,23));disc->setStyleSheet("background:rgba(201,220,175,22);border:1px solid rgba(201,220,175,35);border-radius:12px;");head->addWidget(disc);
    auto* labels=new QVBoxLayout;labels->setSpacing(4);title_=new QLabel("雾岸");title_->setObjectName("musicTitle");title_->setStyleSheet("font-size:14px;font-weight:600;");labels->addWidget(title_);
    status_=new QLabel("原创轻音乐 · 点一曲，慢下来");status_->setObjectName("musicStatus");status_->setStyleSheet("font-size:10px;color:#aac09c;");labels->addWidget(status_);head->addLayout(labels,1);
    play_=new QPushButton("播放");play_->setObjectName("musicPlay");play_->setIcon(IconHsx::get("play","#31523b"));play_->setIconSize(QSize(14,14));head->addWidget(play_);layout->addLayout(head);
    progress_=new QSlider(Qt::Horizontal);progress_->setObjectName("musicProgress");progress_->setRange(0,0);progress_->setAccessibleName("播放进度");layout->addWidget(progress_);
    auto* controls=new QHBoxLayout;auto* choose=new QPushButton("本地音乐");choose->setObjectName("musicChoose");choose->setIcon(IconHsx::get("folder","#b7c9aa"));choose->setIconSize(QSize(13,13));controls->addWidget(choose);controls->addStretch();
    auto* volumeLabel=new QLabel("音量");volumeLabel->setStyleSheet("font-size:9px;color:#a9bc9b;");controls->addWidget(volumeLabel);auto* volume=new QSlider(Qt::Horizontal);volume->setObjectName("musicVolume");volume->setRange(0,100);volume->setValue(35);volume->setFixedWidth(76);volume->setAccessibleName("音乐音量");controls->addWidget(volume);layout->addLayout(controls);
    player_=new QMediaPlayer(this);player_->setObjectName("loginMusicPlayer");audio_=new QAudioOutput(this);audio_->setObjectName("loginAudioOutput");audio_->setVolume(.35f);player_->setAudioOutput(audio_);player_->setLoops(QMediaPlayer::Infinite);
    connect(play_,&QPushButton::clicked,this,[this]{if(player_->playbackState()==QMediaPlayer::PlayingState)player_->pause();else player_->play();});connect(choose,&QPushButton::clicked,this,[this]{selectMusic();});
    connect(volume,&QSlider::valueChanged,this,[this](int value){audio_->setVolume(value/100.0f);});connect(player_,&QMediaPlayer::playbackStateChanged,this,[this]{refresh();});
    connect(player_,&QMediaPlayer::durationChanged,this,[this](qint64 value){progress_->setRange(0,static_cast<int>(value/1000));});connect(player_,&QMediaPlayer::positionChanged,this,[this](qint64 value){if(!progress_->isSliderDown())progress_->setValue(static_cast<int>(value/1000));});
    connect(progress_,&QSlider::sliderMoved,this,[this](int value){player_->setPosition(value*1000LL);});connect(player_,&QMediaPlayer::errorOccurred,this,[this](QMediaPlayer::Error,const QString& text){status_->setText("音乐无法播放，请选择其它文件");status_->setToolTip(text);});
    player_->setSource(QUrl("qrc:/music/mist.wav"));
}
void LoginMusicHsx::refresh(){const bool playing=player_->playbackState()==QMediaPlayer::PlayingState;play_->setText(playing?"暂停":"播放");play_->setIcon(IconHsx::get(playing?"pause":"play","#31523b"));status_->setText(playing?"正在播放 · 循环聆听":player_->playbackState()==QMediaPlayer::PausedState?"已暂停 · 风景依然在这里":"原创轻音乐 · 点一曲，慢下来");}
void LoginMusicHsx::stop(){player_->stop();}
void LoginMusicHsx::selectMusic(){const auto file=QFileDialog::getOpenFileName(this,"选择一首音乐",{},"音频文件 (*.mp3 *.wav *.flac *.ogg *.m4a)");if(file.isEmpty())return;player_->stop();title_->setText(QFileInfo(file).completeBaseName().left(20));title_->setToolTip(file);player_->setSource(QUrl::fromLocalFile(file));player_->play();}
}
