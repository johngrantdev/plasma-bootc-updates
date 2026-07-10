#include <QGuiApplication>

#include <KLocalizedString>

#include "notifier.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    KLocalizedString::setApplicationDomain("bootcupdates");
    app.setDesktopFileName(QStringLiteral("bootcupdates"));

    Notifier notifier;
    return app.exec();
}
