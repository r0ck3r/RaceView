#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QStringList>
#include <QVector>

class MpvPlayer;
class MpvWidget;
class SyncController;
class OsdWidget;
class QPushButton;
class QSlider;
class QLabel;
class QDoubleSpinBox;
class QVBoxLayout;
class QCloseEvent;
class QResizeEvent;
class QWidget;

class PlayerWindow : public QMainWindow {
    Q_OBJECT
public:
    PlayerWindow(int index, MpvPlayer *player, const QString &role,
                 SyncController *sync, QWidget *parent = nullptr);

    void setFilePath(const QString &path);
    QImage grabVideo();
    void ensureRenderReady();
    void setFullscreen(bool on);

protected:
    void keyPressEvent(QKeyEvent *ev) override;
    void keyReleaseEvent(QKeyEvent *ev) override;
    void closeEvent(QCloseEvent *ev) override;
    void changeEvent(QEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;
    void resizeEvent(QResizeEvent *ev) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;

signals:
    void closed();

private slots:
    void updateBar();
    void holdRepeat();

private:
    void toggleFullscreen();
    void cancelSeekHold();
    void buildControls(QVBoxLayout *lay);
    void refreshAudio();
    void openAudioDialog();
    void updateAudioButton();
    void showControls();
    void positionBar();
    void showOsd(double target, double delta);
    void showVolumeOsd(int percent);
    void positionOsd();

    int m_index;
    MpvPlayer *m_player;
    MpvWidget *m_widget;
    SyncController *m_sync;
    QString m_role;
    bool m_fullscreen = false;

    QWidget *m_container = nullptr;
    QVBoxLayout *m_containerLayout = nullptr;
    QWidget *m_barWidget = nullptr;
    QPushButton *m_playBtn = nullptr;
    QPushButton *m_fullBtn = nullptr;
    QSlider *m_seek = nullptr;
    QLabel *m_timeLabel = nullptr;
    QPushButton *m_audioBtn = nullptr;
    QStringList m_audioLabels;
    QVector<int> m_audioIds;
    int m_audioSel = -1;
    QPushButton *m_mute = nullptr;
    QSlider *m_volSlider = nullptr;
    QDoubleSpinBox *m_offset = nullptr;
    OsdWidget *m_osd = nullptr;
    QString m_osdDelta;
    QTimer m_timer;
    QTimer m_hideTimer;
    QTimer m_osdTimer;
    QTimer m_holdTimer;
    double m_holdDt = 0.0;
    bool m_dragging = false;
    int m_audioTries = 0;
};
