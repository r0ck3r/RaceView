#pragma once

#include <QObject>
#include <QString>

class SyncController;

// Единая MPRIS2-регистрация на процесс: org.mpris.MediaPlayer2.raceview.
// Количество видео-окон не влияет: сервис всегда один.
class Mpris : public QObject {
    Q_OBJECT
public:
    explicit Mpris(SyncController *sync, QObject *parent = nullptr);

    bool registerService();

    SyncController *sync() const { return m_sync; }

signals:
    void raiseRequested();
    void quitRequested();
    void openUriRequested(const QString &uri);

private:
    SyncController *m_sync;
};
