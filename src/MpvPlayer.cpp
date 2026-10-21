#include "MpvPlayer.h"
#include <QByteArray>
#include <vector>
#include <cstring>

MpvPlayer::~MpvPlayer() {
    if (m_handle)
        mpv_terminate_destroy(m_handle);
}

bool MpvPlayer::initialize() {
    m_handle = mpv_create();
    if (!m_handle)
        return false;

    const bool headless = qEnvironmentVariableIsSet("RACEVIEW_HEADLESS");
    const bool debug = qEnvironmentVariableIsSet("RACEVIEW_DEBUG");
    // auto-copy обязателен: zero-copy hwaccel (nvdec/vaapi) не отдаёт кадры
    // в OpenGL render-API libmpv -> вместо видео чёрный экран при звуке.
    const QByteArray hwdec =
        qEnvironmentVariable("RACEVIEW_HWDEC", "auto-copy").toUtf8();
    const char *opts[][2] = {
        {"vo", headless ? "null" : "libmpv"},
        {"terminal", debug ? "yes" : "no"},
        {"msg-level", debug ? "all=v" : "all=no"},
        {"idle", "yes"},
        {"keep-open", "yes"},
        {"input-default-bindings", "no"},
        {"input-vo-keyboard", "no"},
        {"hwdec", hwdec.constData()},
    };
    for (auto &o : opts)
        mpv_set_option_string(m_handle, o[0], o[1]);

    if (mpv_initialize(m_handle) < 0) {
        mpv_destroy(m_handle);
        m_handle = nullptr;
        return false;
    }
    return true;
}

void MpvPlayer::loadFile(const QString &path) {
    command({"loadfile", path, "replace"});
}

void MpvPlayer::stop() {
    command({"stop"});
}

void MpvPlayer::command(const QStringList &args) {
    if (!m_handle)
        return;
    QVector<QByteArray> parts;
    parts.reserve(args.size() + 1);
    for (const QString &a : args)
        parts.append(a.toUtf8());
    std::vector<const char *> argv;
    argv.reserve(parts.size());
    for (auto &p : parts)
        argv.push_back(p.constData());
    argv.push_back(nullptr);
    mpv_command(m_handle, argv.data());
}

void MpvPlayer::commandAsync(const QStringList &args) {
    if (!m_handle)
        return;
    QVector<QByteArray> parts;
    parts.reserve(args.size());
    for (const QString &a : args)
        parts.append(a.toUtf8());
    std::vector<const char *> argv;
    argv.reserve(parts.size());
    for (auto &p : parts)
        argv.push_back(p.constData());
    argv.push_back(nullptr);
    mpv_command_async(m_handle, 0, argv.data());
}

void MpvPlayer::drainEvents() {
    if (!m_handle)
        return;
    while (mpv_event *ev = mpv_wait_event(m_handle, 0))
        if (ev->event_id == MPV_EVENT_NONE)
            break;
}

void MpvPlayer::setPaused(bool paused) {
    if (!m_handle)
        return;
    int flag = paused ? 1 : 0;
    mpv_set_property(m_handle, "pause", MPV_FORMAT_FLAG, &flag);
}

bool MpvPlayer::isPaused() const {
    if (!m_handle)
        return true;
    int flag = 1;
    if (mpv_get_property(m_handle, "pause", MPV_FORMAT_FLAG, &flag) < 0)
        return true;
    return flag != 0;
}

void MpvPlayer::setSpeed(double speed) {
    if (!m_handle)
        return;
    double v = speed;
    mpv_set_property(m_handle, "speed", MPV_FORMAT_DOUBLE, &v);
}

void MpvPlayer::setVolume(double volume) {
    if (!m_handle)
        return;
    double v = volume;
    mpv_set_property(m_handle, "volume", MPV_FORMAT_DOUBLE, &v);
}

double MpvPlayer::volume() const {
    if (!m_handle)
        return 100.0;
    double v = 100.0;
    mpv_get_property(m_handle, "volume", MPV_FORMAT_DOUBLE, &v);
    return v;
}

void MpvPlayer::setMuted(bool muted) {
    if (!m_handle)
        return;
    int flag = muted ? 1 : 0;
    mpv_set_property(m_handle, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvPlayer::seekAbsolute(double t) {
    commandAsync({"seek", QString::number(t, 'f', 3), "absolute+exact"});
}

void MpvPlayer::frameStep(bool backward) {
    command({backward ? "frame-back-step" : "frame-step"});
}

QVector<MpvPlayer::AudioTrack> MpvPlayer::audioTracks() const {
    QVector<AudioTrack> out;
    if (!m_handle)
        return out;
    mpv_node node;
    if (mpv_get_property(m_handle, "track-list", MPV_FORMAT_NODE, &node) < 0)
        return out;
    if (node.format != MPV_FORMAT_NODE_ARRAY) {
        mpv_free_node_contents(&node);
        return out;
    }
    mpv_node_list *list = node.u.list;
    for (int i = 0; i < list->num; ++i) {
        mpv_node &el = list->values[i];
        if (el.format != MPV_FORMAT_NODE_MAP)
            continue;
        mpv_node_list *m = el.u.list;
        QString type, title, lang;
        long long id = -1;
        bool selected = false, def = false;
        for (int k = 0; k < m->num; ++k) {
            const char *key = m->keys[k];
            mpv_node &v = m->values[k];
            if (!std::strcmp(key, "type") && v.format == MPV_FORMAT_STRING)
                type = QString::fromUtf8(v.u.string);
            else if (!std::strcmp(key, "id") && v.format == MPV_FORMAT_INT64)
                id = v.u.int64;
            else if (!std::strcmp(key, "title") && v.format == MPV_FORMAT_STRING)
                title = QString::fromUtf8(v.u.string);
            else if (!std::strcmp(key, "lang") && v.format == MPV_FORMAT_STRING)
                lang = QString::fromUtf8(v.u.string);
            else if (!std::strcmp(key, "selected") && v.format == MPV_FORMAT_FLAG)
                selected = v.u.flag != 0;
            else if (!std::strcmp(key, "default") && v.format == MPV_FORMAT_FLAG)
                def = v.u.flag != 0;
        }
        if (type == "audio") {
            AudioTrack t;
            t.id = int(id);
            t.title = title;
            t.lang = lang;
            t.selected = selected;
            t.isDefault = def;
            out.push_back(t);
        }
    }
    mpv_free_node_contents(&node);
    return out;
}

void MpvPlayer::setAudioId(int id) {
    if (!m_handle)
        return;
    long long v = id;
    mpv_set_property(m_handle, "aid", MPV_FORMAT_INT64, &v);
}

int MpvPlayer::audioId() const {
    if (!m_handle)
        return -1;
    long long v = -1;
    if (mpv_get_property(m_handle, "aid", MPV_FORMAT_INT64, &v) < 0)
        return -1;
    return int(v);
}

double MpvPlayer::timePos() const {
    return getDouble("time-pos");
}

double MpvPlayer::duration() const {
    return getDouble("duration");
}

double MpvPlayer::getDouble(const char *name) const {
    if (!m_handle)
        return -1.0;
    double v = -1.0;
    if (mpv_get_property(m_handle, name, MPV_FORMAT_DOUBLE, &v) < 0)
        return -1.0;
    return v;
}
