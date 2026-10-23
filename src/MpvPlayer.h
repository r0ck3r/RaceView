#pragma once

#include <mpv/client.h>
#include <QString>
#include <QStringList>
#include <QVector>

class MpvPlayer {
public:
    struct AudioTrack {
        int id = 0;
        QString title;
        QString lang;
        bool selected = false;
        bool isDefault = false;
    };

    // hwdec value used by initialize(): RACEVIEW_HWDEC wins, then the GUI
    // choice stored in the settings, then the built-in copy-only list.
    static QString defaultHwdec();

    MpvPlayer() = default;
    ~MpvPlayer();
    MpvPlayer(const MpvPlayer &) = delete;
    MpvPlayer &operator=(const MpvPlayer &) = delete;

    bool initialize();
    mpv_handle *handle() const { return m_handle; }

    void loadFile(const QString &path);
    void stop();
    void command(const QStringList &args);
    void commandAsync(const QStringList &args);
    void drainEvents();

    void setPaused(bool paused);
    bool isPaused() const;
    void setSpeed(double speed);
    // Runtime hwdec switch: mpv reinitializes the decoder, playback continues.
    void setHwdec(const QString &api);
    // The API mpv actually ended up using, e.g. "vdpau-copy" ("" = software).
    QString hwdecCurrent() const;
    void setVolume(double volume);
    double volume() const;
    void setMuted(bool muted);
    void seekAbsolute(double t);
    void frameStep(bool backward);

    QVector<AudioTrack> audioTracks() const;
    void setAudioId(int id);
    int audioId() const;

    double timePos() const;
    double duration() const;

private:
    void observeProperties();

    mpv_handle *m_handle = nullptr;

    // Asynchronously cached properties. The GUI thread never calls the
    // blocking mpv_get_property/mpv_set_property on the hot path: reads come
    // from these caches (refreshed in drainEvents) and writes are async, so a
    // long-running exact seek in the mpv core can never freeze the UI.
    double m_timePos = -1.0;
    double m_duration = -1.0;
    bool m_pause = false;
    QString m_hwdecCurrent;
};
