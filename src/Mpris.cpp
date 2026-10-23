#include "Mpris.h"

#ifdef RACEVIEW_MPRIS

#include "MprisAdaptors.h"
#include <QDBusConnection>

Mpris::Mpris(SyncController *sync, QObject *parent)
    : QObject(parent), m_sync(sync) {
    new MprisAppAdaptor(this);
    new MprisPlayerAdaptor(this);
}

bool Mpris::registerService() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;
    const QString service = QStringLiteral("org.mpris.MediaPlayer2.raceview");
    if (!bus.registerService(service))
        return false; // already registered by another instance
    return bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this,
                              QDBusConnection::ExportAdaptors);
}

#else // MPRIS is unavailable (Windows/macOS): no-op stub

Mpris::Mpris(SyncController *sync, QObject *parent)
    : QObject(parent), m_sync(sync) {}

bool Mpris::registerService() { return false; }

#endif
