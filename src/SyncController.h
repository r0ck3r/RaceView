#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QStringList>
#include <QVector>
#include <memory>
#include <vector>

#include "MpvPlayer.h"

class MpvPlayer;

class SyncController : public QObject {
    Q_OBJECT
public:
    explicit SyncController(QObject *parent = nullptr);
    ~SyncController() override;

    bool start(const QStringList &paths);
    bool preparePlayers(int count);
    void stopAll();

    bool isStarted() const { return m_started; }
    bool seekHolding() const { return m_seekHolding; }
    double seekTarget() const { return m_seekTarget; }
    int trackCount() const { return m_tracks.size(); }
    MpvPlayer *player(int index) const;
    bool isTrackEnabled(int index) const;
    bool isPlaying() const;
    // What the user asked for: hold-to-seek auto-pauses the players, which is
    // not a pause the user requested, so it must not look like one in the UI.
    bool isPlayingIntent() const {
        return m_seekHolding ? m_seekWasPlaying : isPlaying();
    }

    void togglePlay();
    void play();
    void pause();
    void seekTo(double t);
    void seekRelative(double dt);
    void nudgeSeek(double dt);
    void endSeekHold();
    void previewSeek(double t);
    void frameStep(bool backward);

    void setSpeed(double speed);
    void changeSpeed(double factor);
    double speed() const { return m_speed; }

    void changeVolume(double delta);
    double volume() const { return m_volume; }

    void setTrackMuted(int index, bool muted);
    // Decoder choice (mpv hwdec value, "" = built-in default) for every track.
    void setHwdec(const QString &api);
    QString hwdecCurrent() const;
    void setTrackVolume(int index, double volume);
    double trackVolume(int index) const;
    void setTrackOffset(int index, double offset);
    double trackOffset(int index) const;

    QVector<MpvPlayer::AudioTrack> audioTracks(int index) const;
    void setTrackAudio(int index, int aid);

    double masterTime() const;
    double masterDuration() const;
    QString masterPath() const;

signals:
    void startedChanged(bool started);
    void playbackStateChanged();
    void seeked(double target);
    void seekPreview(double target);
    void seekProgress(double target, double delta);

private slots:
    void tick();

private:
    void applySeek(double t);
    bool ensureTracks(int count);

    struct Track {
        std::unique_ptr<MpvPlayer> player;
        bool enabled = false;
        bool muted = false;
        double volume = 100.0;
        double offset = 0.0;
        bool seekPending = false;
        qint64 pendingMs = 0;
        bool nudging = false;
    };

    std::vector<Track> m_tracks;
    QStringList m_paths;
    QTimer m_timer;
    QElapsedTimer m_clock;
    bool m_started = false;
    double m_speed = 1.0;
    double m_volume = 100.0;

    // Hold-to-seek: while an arrow key is held we only accumulate a target and
    // drive the OSD; the real (exact) seek is issued once, on key release.
    bool m_seekHolding = false;
    bool m_seekWasPlaying = false;
    double m_seekTarget = 0.0;
    double m_seekDelta = 0.0;
    qint64 m_seekLastNudgeMs = 0;
};
