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
    double getDouble(const char *name) const;

    mpv_handle *m_handle = nullptr;
};
