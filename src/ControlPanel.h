#pragma once

#include <QWidget>
#include <QVector>
#include <QTimer>

class SyncController;
class PlayerWindow;
class QLineEdit;
class QPushButton;
class QSlider;
class QLabel;
class QComboBox;
class QHBoxLayout;
class QVBoxLayout;
class QCloseEvent;

class ControlPanel : public QWidget {
    Q_OBJECT
public:
    explicit ControlPanel(SyncController *sync, QWidget *parent = nullptr);
    ~ControlPanel() override;

    void setPaths(const QStringList &paths);
    void startOrStop();

    void raiseWindows();
    void requestQuit();
    void openUri(const QString &uri);

protected:
    void closeEvent(QCloseEvent *ev) override;

private slots:
    void browse(QLineEdit *target);
    void browseFolder();
    void togglePlay();
    void updateUi();
    void showAbout();

private:
    struct FileRow {
        QHBoxLayout *layout = nullptr;
        QLabel *label = nullptr;
        QLineEdit *edit = nullptr;
        QPushButton *browse = nullptr;
        QPushButton *remove = nullptr;
    };

    static QString trackName(int index);
    void addPathRow();
    void removeRow(int index);
    void refreshRows();
    void ensureWindows(int count);
    void applySpeed();
    void setStartedUi(bool started);
    void onWindowClosed();
    void quitApp();
    void restartApp();
    void toggleAllFullscreen();

    SyncController *m_sync;
    QVector<PlayerWindow *> m_windows;
    bool m_quitting = false;
    bool m_allFullscreen = false;

    QVBoxLayout *m_rowsLayout = nullptr;
    QVector<FileRow> m_rows;

    QPushButton *m_startBtn = nullptr;
    QPushButton *m_playBtn = nullptr;
    QPushButton *m_fullAllBtn = nullptr;
    QSlider *m_seek = nullptr;
    QLabel *m_timeLabel = nullptr;
    QSlider *m_speed = nullptr;
    QLabel *m_speedLabel = nullptr;
    QComboBox *m_langCombo = nullptr;
    QComboBox *m_hwdecCombo = nullptr;

    QWidget *m_controls = nullptr;

    bool m_dragging = false;
    QTimer m_uiTimer;
};
