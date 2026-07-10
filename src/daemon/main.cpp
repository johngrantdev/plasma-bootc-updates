#include <QCoreApplication>
#include <QDBusConnection>

#include <unistd.h>

#include "updater.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (geteuid() != 0) {
        qCritical("bootcupdatesd must run as root (it is D-Bus activated, do not run it manually)");
        return 1;
    }

    Updater updater;
    auto bus = QDBusConnection::systemBus();
    if (!bus.registerObject(QStringLiteral("/io/github/jayelg/BootcUpdates1"), &updater,
                            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals)) {
        qCritical("Failed to register D-Bus object");
        return 1;
    }
    if (!bus.registerService(QStringLiteral("io.github.jayelg.BootcUpdates1"))) {
        qCritical("Failed to register D-Bus service name");
        return 1;
    }

    return app.exec();
}
