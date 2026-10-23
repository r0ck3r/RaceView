#include "MpvPlayer.h"
#include <QByteArray>
#include <QSettings>
#include <vector>
#include <cstring>

MpvPlayer::~MpvPlayer() {
    if (m_handle)
        mpv_terminate_destroy(m_handle);
}

// Only the -copy hwaccels work here: zero-copy (nvdec/vaapi) never hands
// frames to libmpv's OpenGL render API, so audio plays over a black image.
// The API list is explicit instead of auto-copy, which picks FFmpeg's Vulkan
// decoder and corrupts H.264 frames on some drivers (NVIDIA) - periodic
// picture breakdown (SSIM 0.97 against software decode of the same file).
// mpv ignores unknown names, so the list works with any libmpv version, and
// when no API initializes mpv falls back to software decoding.
QString MpvPlayer::defaultHwdec() {
    const QByteArray env = qgetenv("RACEVIEW_HWDEC");
    if (!env.isEmpty())
        return QString::fromUtf8(env);
    const QString saved =
        QSettings(QStringLiteral("raceview"), QStringLiteral("raceview"))
            .value(QStringLiteral("hwdec"))
            .toString();
    if (!saved.isEmpty())
        return saved;
#if defined(Q_OS_WIN)
    return QStringLiteral("d3d11va-copy,nvdec-copy,cuda-copy,dxva2-copy");
#else
    return QStringLiteral("vaapi-copy,vdpau-copy,nvdec-copy,cuda-copy");
#endif
}

bool MpvPlayer::initialize() {
    m_handle = mpv_create();
    if (!m_handle)
        return false;

    const bool headless = qEnvironmentVariableIsSet("RACEVIEW_HEADLESS");
    const bool debug = qEnvironmentVariableIsSet("RACEVIEW_DEBUG");
    const QByteArray hwdec = defaultHwdec().toUtf8();
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
    observeProperties();
    return true;
}

void MpvPlayer::observeProperties() {
    if (!m_handle)
        return;
    mpv_observe_property(m_handle, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_handle, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_handle, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_handle, 0, "hwdec-current", MPV_FORMAT_STRING);
}

void MpvPlayer::loadFile(const QString &path) {
    m_timePos = -1.0;
    m_duration = -1.0;
    commandAsync({"loadfile", path, "replace"});
}

void MpvPlayer::stop() {
    m_timePos = -1.0;
    m_duration = -1.0;
    commandAsync({"stop"});
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
    while (mpv_event *ev = mpv_wait_event(m_handle, 0)) {
        if (ev->event_id == MPV_EVENT_NONE)
            break;
        if (ev->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            auto *p = static_cast<mpv_event_property *>(ev->data);
            if (!p || !p->name)
                continue;
            if (std::strcmp(p->name, "time-pos") == 0) {
                if (p->format == MPV_FORMAT_DOUBLE && p->data)
                    m_timePos = *static_cast<double *>(p->data);
                else if (p->format == MPV_FORMAT_NONE)
                    m_timePos = -1.0;
            } else if (std::strcmp(p->name, "duration") == 0) {
                if (p->format == MPV_FORMAT_DOUBLE && p->data)
                    m_duration = *static_cast<double *>(p->data);
                else if (p->format == MPV_FORMAT_NONE)
                    m_duration = -1.0;
            } else if (std::strcmp(p->name, "pause") == 0) {
                if (p->format == MPV_FORMAT_FLAG && p->data)
                    m_pause = *static_cast<int *>(p->data) != 0;
            } else if (std::strcmp(p->name, "hwdec-current") == 0) {
                // The string belongs to the event: reading it is fine, but
                // mpv_free() on it wrecks mpv's talloc headers (abort in
                // ta_dbg_check_header).
                if (p->format == MPV_FORMAT_STRING && p->data)
                    m_hwdecCurrent =
                        QString::fromUtf8(*static_cast<char **>(p->data));
                else if (p->format == MPV_FORMAT_NONE)
                    m_hwdecCurrent.clear();
            }
        }
    }
}

void MpvPlayer::setPaused(bool paused) {
    if (!m_handle)
        return;
    int flag = paused ? 1 : 0;
    m_pause = paused; // optimistic: keeps isPlaying() correct without a round-trip
    mpv_set_property_async(m_handle, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

bool MpvPlayer::isPaused() const { return m_pause; }

void MpvPlayer::setSpeed(double speed) {
    if (!m_handle)
        return;
    double v = speed;
    mpv_set_property_async(m_handle, 0, "speed", MPV_FORMAT_DOUBLE, &v);
}

void MpvPlayer::setHwdec(const QString &api) {
    if (!m_handle)
        return;
    m_hwdecCurrent.clear(); // the real value arrives via the property observer
    QByteArray value = api.toUtf8();
    const char *str = value.constData();
    mpv_set_property_async(m_handle, 0, "hwdec", MPV_FORMAT_STRING, &str);
}

QString MpvPlayer::hwdecCurrent() const { return m_hwdecCurrent; }

void MpvPlayer::setVolume(double volume) {
    if (!m_handle)
        return;
    double v = volume;
    mpv_set_property_async(m_handle, 0, "volume", MPV_FORMAT_DOUBLE, &v);
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
    mpv_set_property_async(m_handle, 0, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvPlayer::seekAbsolute(double t) {
    commandAsync({"seek", QString::number(t, 'f', 3), "absolute+exact"});
}

void MpvPlayer::frameStep(bool backward) {
    commandAsync({backward ? "frame-back-step" : "frame-step"});
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

double MpvPlayer::timePos() const { return m_timePos; }

double MpvPlayer::duration() const { return m_duration; }
