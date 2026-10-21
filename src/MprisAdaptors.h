#pragma once

#ifdef RACEVIEW_MPRIS

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QFileInfo>
#include <QUrl>

#include "Mpris.h"
#include "SyncController.h"

class MprisPlayerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(bool CanGoNext READ canGoNext CONSTANT)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious CONSTANT)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)
public:
    explicit MprisPlayerAdaptor(Mpris *m)
        : QDBusAbstractAdaptor(m), m_mpris(m) {
        SyncController *s = m_mpris->sync();
        connect(s, &SyncController::playbackStateChanged, this, [this] {
            sendChanged({"PlaybackStatus"});
        });
        connect(s, &SyncController::startedChanged, this, [this] {
            sendChanged({"PlaybackStatus", "Metadata", "CanPlay", "CanPause",
                         "CanSeek", "CanControl"});
        });
        m_posTimer.setInterval(1000);
        connect(&m_posTimer, &QTimer::timeout, this,
                [this] { sendChanged({"Position"}); });
        m_posTimer.start();
    }

    QString playbackStatus() const {
        SyncController *s = m_mpris->sync();
        if (!s->isStarted())
            return QStringLiteral("Stopped");
        return s->isPlaying() ? QStringLiteral("Playing")
                              : QStringLiteral("Paused");
    }
    QString loopStatus() const { return QStringLiteral("None"); }
    void setLoopStatus(const QString &) {}
    double rate() const { return m_mpris->sync()->speed(); }
    void setRate(double r) { m_mpris->sync()->setSpeed(r); }
    double minimumRate() const { return 0.1; }
    double maximumRate() const { return 4.0; }

    QVariantMap metadata() const {
        QVariantMap m;
        SyncController *s = m_mpris->sync();
        if (!s->isStarted())
            return m;
        m[QStringLiteral("mpris:trackid")] = QVariant::fromValue(
            QDBusObjectPath("/org/mpris/MediaPlayer2/TrackList/Track1"));
        const double d = s->masterDuration();
        if (d > 0)
            m[QStringLiteral("mpris:length")] = qlonglong(d * 1e6);
        const QString path = s->masterPath();
        if (!path.isEmpty()) {
            m[QStringLiteral("xesam:title")] = QFileInfo(path).fileName();
            m[QStringLiteral("xesam:url")] =
                QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath())
                    .toString();
        }
        return m;
    }

    qlonglong position() const {
        const double t = m_mpris->sync()->masterTime();
        return t > 0 ? qlonglong(t * 1e6) : 0;
    }

    bool canGoNext() const { return false; }
    bool canGoPrevious() const { return false; }
    bool canPlay() const { return m_mpris->sync()->isStarted(); }
    bool canPause() const { return m_mpris->sync()->isStarted(); }
    bool canSeek() const { return m_mpris->sync()->isStarted(); }
    bool canControl() const { return m_mpris->sync()->isStarted(); }

public slots:
    void Play() { m_mpris->sync()->play(); }
    void Pause() { m_mpris->sync()->pause(); }
    void PlayPause() { m_mpris->sync()->togglePlay(); }
    void Stop() { m_mpris->sync()->stopAll(); }
    void Next() {}
    void Previous() {}
    void Seek(qlonglong offsetUs) {
        m_mpris->sync()->seekRelative(double(offsetUs) / 1e6);
    }
    void SetPosition(const QDBusObjectPath &, qlonglong timeUs) {
        m_mpris->sync()->seekTo(double(timeUs) / 1e6);
    }
    void OpenUri(const QString &uri) { emit m_mpris->openUriRequested(uri); }

private:
    QVariant currentValue(const QString &name) const {
        if (name == QLatin1String("PlaybackStatus"))
            return playbackStatus();
        if (name == QLatin1String("Metadata"))
            return metadata();
        if (name == QLatin1String("Position"))
            return position();
        if (name == QLatin1String("CanPlay"))
            return canPlay();
        if (name == QLatin1String("CanPause"))
            return canPause();
        if (name == QLatin1String("CanSeek"))
            return canSeek();
        if (name == QLatin1String("CanControl"))
            return canControl();
        return {};
    }

    void sendChanged(const QStringList &names) {
        QVariantMap changed;
        for (const QString &n : names)
            changed.insert(n, currentValue(n));
        if (changed.isEmpty())
            return;
        QDBusMessage msg = QDBusMessage::createSignal(
            QStringLiteral("/org/mpris/MediaPlayer2"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));
        msg << QStringLiteral("org.mpris.MediaPlayer2.Player") << changed
            << QStringList();
        QDBusConnection::sessionBus().send(msg);
    }

    Mpris *m_mpris;
    QTimer m_posTimer;
};

class MprisAppAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanHide READ canHide CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)
    Q_PROPERTY(bool Fullscreen READ fullscreen WRITE setFullscreen)
    Q_PROPERTY(bool FullscreenChangeable READ fullscreenChangeable CONSTANT)
public:
    explicit MprisAppAdaptor(Mpris *m)
        : QDBusAbstractAdaptor(m), m_mpris(m) {}

    bool canQuit() const { return true; }
    bool canHide() const { return false; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("RaceView"); }
    QString desktopEntry() const { return QStringLiteral("raceview"); }
    QStringList supportedUriSchemes() const { return {QStringLiteral("file")}; }
    QStringList supportedMimeTypes() const {
        return {QStringLiteral("video/x-matroska"), QStringLiteral("video/mp4"),
                QStringLiteral("video/*")};
    }
    bool fullscreen() const { return false; }
    void setFullscreen(bool) {}
    bool fullscreenChangeable() const { return false; }

public slots:
    void Raise() { emit m_mpris->raiseRequested(); }
    void Quit() { emit m_mpris->quitRequested(); }

private:
    Mpris *m_mpris;
};

#endif // RACEVIEW_MPRIS
