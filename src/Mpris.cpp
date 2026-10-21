#include "Mpris.h"
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
        return false; // уже зарегистрирован другим экземпляром
    return bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this,
                              QDBusConnection::ExportAdaptors);
}
