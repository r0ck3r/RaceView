#pragma once

#include <QObject>
#include <QString>

class SyncController;

// Single MPRIS2 registration per process: org.mpris.MediaPlayer2.raceview.
// The number of video windows is irrelevant: there is always one service.
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
