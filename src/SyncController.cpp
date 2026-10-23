#include "SyncController.h"
#include <cstdio>
#include "MpvPlayer.h"

#include <QtMath>

SyncController::SyncController(QObject *parent) : QObject(parent) {
    m_clock.start();
    m_timer.setInterval(200);
    connect(&m_timer, &QTimer::timeout, this, &SyncController::tick);
}

SyncController::~SyncController() = default;

bool SyncController::ensureTracks(int count) {
    while (int(m_tracks.size()) < count) {
        Track tr;
        tr.player = std::make_unique<MpvPlayer>();
        if (!tr.player->initialize())
            return false;
        m_tracks.push_back(std::move(tr));
    }
    return true;
}

MpvPlayer *SyncController::player(int index) const {
    if (index < 0 || index >= int(m_tracks.size()))
        return nullptr;
    return m_tracks[index].player.get();
}

void SyncController::setHwdec(const QString &api) {
    for (auto &tr : m_tracks)
        if (tr.player)
            tr.player->setHwdec(api);
}

QString SyncController::hwdecCurrent() const {
    for (const auto &tr : m_tracks)
        if (tr.player && !tr.player->hwdecCurrent().isEmpty())
            return tr.player->hwdecCurrent();
    return QString();
}

bool SyncController::isTrackEnabled(int index) const {
    return index >= 0 && index < int(m_tracks.size()) && m_tracks[index].enabled;
}

bool SyncController::preparePlayers(int count) {
    return ensureTracks(count);
}

bool SyncController::start(const QStringList &paths) {
    stopAll();

    if (!ensureTracks(paths.size()))
        return false;

    m_paths = paths;
    for (int i = 0; i < int(m_tracks.size()); ++i) {
        Track &tr = m_tracks[i];
        tr.enabled = i < paths.size() && !paths[i].isEmpty();
        tr.seekPending = false;
        tr.nudging = false;
        tr.muted = false;
        tr.offset = 0.0;
        if (!tr.enabled) {
            if (tr.player)
                tr.player->stop();
            continue;
        }
        tr.player->loadFile(paths[i]);
        tr.player->setPaused(false);
        tr.player->setSpeed(m_speed);
        tr.player->setVolume(tr.volume);
        tr.player->setMuted(tr.muted);
    }

    m_started = true;
    m_timer.start();
    emit startedChanged(true);
    emit playbackStateChanged();
    return true;
}

void SyncController::stopAll() {
    m_timer.stop();
    m_seekHolding = false;
    m_seekWasPlaying = false;
    m_seekDelta = 0.0;
    if (!m_started)
        return;
    for (const Track &tr : m_tracks)
        if (tr.enabled && tr.player)
            tr.player->stop();
    m_started = false;
    m_paths.clear();
    emit startedChanged(false);
    emit playbackStateChanged();
}

bool SyncController::isPlaying() const {
    if (!m_started || m_tracks.empty())
        return false;
    return !m_tracks[0].player->isPaused();
}

void SyncController::togglePlay() {
    if (!m_started || m_tracks.empty())
        return;
    // During a scrub hold the players are auto-paused for the gesture; the
    // user-facing state is the intent (m_seekWasPlaying), not the real pause.
    const bool pause = m_seekHolding ? !m_seekWasPlaying
                                     : m_tracks[0].player->isPaused();
    for (const Track &tr : m_tracks)
        if (tr.enabled)
            tr.player->setPaused(!pause);
    if (m_seekHolding)
        m_seekWasPlaying = !pause;
    emit playbackStateChanged();
}

void SyncController::play() {
    if (!m_started || m_tracks.empty())
        return;
    for (const Track &tr : m_tracks)
        if (tr.enabled)
            tr.player->setPaused(false);
    if (m_seekHolding)
        m_seekWasPlaying = true;
    emit playbackStateChanged();
}

void SyncController::pause() {
    if (!m_started || m_tracks.empty())
        return;
    for (const Track &tr : m_tracks)
        if (tr.enabled)
            tr.player->setPaused(true);
    if (m_seekHolding)
        m_seekWasPlaying = false;
    emit playbackStateChanged();
}

void SyncController::applySeek(double t) {
    for (int i = 0; i < int(m_tracks.size()); ++i) {
        Track &tr = m_tracks[i];
        if (!tr.enabled)
            continue;
        tr.seekPending = false;
        double target = (i == 0) ? t : t + tr.offset;
        tr.player->seekAbsolute(qMax(0.0, target));
    }
}

void SyncController::seekTo(double t) {
    if (!m_started)
        return;
    applySeek(t);
    emit seeked(t);
}

void SyncController::previewSeek(double t) {
    if (!m_started)
        return;
    emit seekPreview(t);
}

void SyncController::seekRelative(double dt) {
    double t = masterTime();
    if (t < 0)
        t = 0;
    seekTo(t + dt);
}

// Hold-to-seek. While an arrow key is held we pause playback and only
// accumulate the requested target, driving the OSD and the progress slider.
// The video itself is NOT seeked during the hold (no mpv traffic at all); the
// single real seek happens in endSeekHold() on key release.
void SyncController::nudgeSeek(double dt) {
    if (!m_started)
        return;
    if (!m_seekHolding) {
        m_seekWasPlaying = isPlaying();
        if (m_seekWasPlaying)
            pause(); // must run before m_seekHolding is set
        m_seekHolding = true;
        double t = masterTime();
        if (t < 0)
            t = 0;
        m_seekTarget = t;
        m_seekDelta = 0.0;
    }
    m_seekTarget = m_seekTarget + dt;
    if (m_seekTarget < 0)
        m_seekTarget = 0;
    const double dur = masterDuration();
    if (dur > 0 && m_seekTarget > dur)
        m_seekTarget = dur;
    m_seekDelta += dt;
    m_seekLastNudgeMs = m_clock.elapsed();
    emit seekProgress(m_seekTarget, m_seekDelta);
}

void SyncController::endSeekHold() {
    if (!m_seekHolding)
        return;
    m_seekHolding = false;
    const double target = m_seekTarget;
    m_seekDelta = 0.0;
    seekTo(target);
    if (m_seekWasPlaying)
        play();
    m_seekWasPlaying = false;
}

void SyncController::frameStep(bool backward) {
    if (!m_started)
        return;
    for (const Track &tr : m_tracks)
        if (tr.enabled)
            tr.player->frameStep(backward);
}

void SyncController::setSpeed(double speed) {
    m_speed = qBound(0.1, speed, 4.0);
    for (const Track &tr : m_tracks)
        if (tr.enabled)
            tr.player->setSpeed(m_speed);
}

void SyncController::changeSpeed(double factor) { setSpeed(m_speed * factor); }

void SyncController::changeVolume(double delta) {
    m_volume = qBound(0.0, m_volume + delta, 130.0);
    for (const Track &tr : m_tracks)
        if (tr.enabled)
            tr.player->setVolume(m_volume);
}

void SyncController::setTrackMuted(int index, bool muted) {
    if (index < 0 || index >= int(m_tracks.size()))
        return;
    m_tracks[index].muted = muted;
    if (m_tracks[index].enabled && m_tracks[index].player)
        m_tracks[index].player->setMuted(muted);
}

void SyncController::setTrackVolume(int index, double volume) {
    if (index < 0 || index >= int(m_tracks.size()))
        return;
    m_tracks[index].volume = qBound(0.0, volume, 130.0);
    if (m_tracks[index].enabled && m_tracks[index].player)
        m_tracks[index].player->setVolume(m_tracks[index].volume);
}

double SyncController::trackVolume(int index) const {
    if (index < 0 || index >= int(m_tracks.size()))
        return 100.0;
    return m_tracks[index].volume;
}

void SyncController::setTrackOffset(int index, double offset) {
    if (index < 0 || index >= int(m_tracks.size()))
        return;
    m_tracks[index].offset = offset;
    if (index > 0 && m_started && m_tracks[index].enabled) {
        double t = masterTime();
        if (t >= 0) {
            m_tracks[index].seekPending = false;
            m_tracks[index].player->seekAbsolute(qMax(0.0, t + offset));
        }
    }
}

QVector<MpvPlayer::AudioTrack> SyncController::audioTracks(int index) const {
    if (index < 0 || index >= int(m_tracks.size()) || !m_tracks[index].enabled ||
        !m_tracks[index].player)
        return {};
    return m_tracks[index].player->audioTracks();
}

void SyncController::setTrackAudio(int index, int aid) {
    if (index < 0 || index >= int(m_tracks.size()))
        return;
    if (m_tracks[index].enabled && m_tracks[index].player)
        m_tracks[index].player->setAudioId(aid);
}

double SyncController::masterTime() const {
    if (m_tracks.empty() || !m_tracks[0].player)
        return -1.0;
    return m_tracks[0].player->timePos();
}

double SyncController::masterDuration() const {
    if (m_tracks.empty() || !m_tracks[0].player)
        return -1.0;
    return m_tracks[0].player->duration();
}

QString SyncController::masterPath() const {
    return m_paths.isEmpty() ? QString() : m_paths.first();
}

void SyncController::tick() {
    if (!m_started || m_tracks.empty())
        return;

    // During a hold-to-seek we deliberately do not touch mpv at all: no event
    // draining, no position reads, no corrective seeks. The OSD is driven purely
    // from the accumulated target and the single real seek happens on release.
    // This keeps the core completely idle while the user scrubs.
    if (m_seekHolding) {
        // Safety net: if a key-release was never delivered (focus loss, etc.)
        // the hold would otherwise leave playback paused forever. Autorepeat is
        // continuous, so a long gap means the gesture effectively ended.
        if (m_clock.elapsed() - m_seekLastNudgeMs > 1500)
            endSeekHold();
        return;
    }

    for (const Track &tr : m_tracks)
        if (tr.enabled && tr.player)
            tr.player->drainEvents();

    const double t = masterTime();
    if (t < 0)
        return;

    const bool debug = qEnvironmentVariableIsSet("RACEVIEW_DEBUG");
    const bool paused = m_tracks[0].player->isPaused();
    for (int i = 1; i < int(m_tracks.size()); ++i) {
        Track &tr = m_tracks[i];
        if (!tr.enabled)
            continue;
        const double target = qMax(0.0, t + tr.offset);
        const double pos = tr.player->timePos();

        if (tr.seekPending) {
            if (m_clock.elapsed() - tr.pendingMs > 700)
                tr.seekPending = false;
            else
                continue;
        }

        const double drift = pos - target;
        const double ad = qAbs(drift);
        char action = '-';
        if (pos < 0 || paused || ad > 0.25) {
            if (tr.nudging) {
                tr.player->setSpeed(m_speed);
                tr.nudging = false;
            }
            tr.player->seekAbsolute(target);
            tr.seekPending = true;
            tr.pendingMs = m_clock.elapsed();
            action = 'S';
        } else if (ad > 0.05) {
            tr.player->setSpeed(drift > 0 ? m_speed * 0.95 : m_speed * 1.05);
            tr.nudging = true;
            action = 'N';
        } else if (tr.nudging) {
            tr.player->setSpeed(m_speed);
            tr.nudging = false;
            action = 'R';
        }
        if (debug)
            fprintf(stderr,
                    "sync t=%.3f track%d pos=%.3f drift=%.3f act=%c\n", t, i,
                    pos, drift, action);
    }
}
