#include "notifier.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDateTime>
#include <QFile>
#include <QProcess>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KNotification>
#include <KSharedConfig>

using namespace Qt::StringLiterals;

namespace
{
const auto RUN_OSTREE = u"/run/ostree"_s;
const auto STAGED = u"/run/ostree/staged-deployment"_s;

QTime configuredRestartTime()
{
    const auto config = KSharedConfig::openConfig(u"falcos-bootc-updatesrc"_s);
    const auto time = QTime::fromString(
        KConfigGroup(config, u"General"_s).readEntry("RestartTime", u"02:00"_s), u"HH:mm"_s);
    return time.isValid() ? time : QTime(2, 0);
}
}

Notifier::Notifier(QObject *parent)
    : QObject(parent)
{
    // Watch the directory, since the staged-deployment file comes and goes.
    m_watcher.addPath(RUN_OSTREE);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &Notifier::evaluate);
    evaluate();
}

void Notifier::evaluate()
{
    const bool staged = QFile::exists(STAGED);
    if (staged && !m_notified) {
        m_notified = true;
        notifyStaged();
    } else if (!staged) {
        m_notified = false;
    }
}

void Notifier::notifyStaged()
{
    auto notification = new KNotification(u"updateStaged"_s);
    notification->setComponentName(u"bootcupdates"_s);
    notification->setTitle(i18n("System update ready"));
    notification->setText(i18n("An update has been staged and will apply on the next restart."));
    notification->setIconName(u"system-software-update"_s);
    notification->setFlags(KNotification::Persistent);

    auto restart = notification->addAction(i18n("Restart Now"));
    connect(restart, &KNotificationAction::activated, this, &Notifier::restartNow);

    const QTime when = configuredRestartTime();
    auto schedule = notification->addAction(i18n("Restart at %1", when.toString(u"HH:mm"_s)));
    connect(schedule, &KNotificationAction::activated, this, &Notifier::scheduleRestart);

    auto open = notification->addDefaultAction(i18n("Open System Updates"));
    connect(open, &KNotificationAction::activated, this, [] {
        QProcess::startDetached(u"systemsettings"_s, {u"kcm_bootcupdates"_s});
    });

    notification->sendEvent();
}

void Notifier::restartNow()
{
    auto msg = QDBusMessage::createMethodCall(u"org.freedesktop.login1"_s, u"/org/freedesktop/login1"_s,
                                              u"org.freedesktop.login1.Manager"_s, u"Reboot"_s);
    msg << true;
    QDBusConnection::systemBus().asyncCall(msg);
}

void Notifier::scheduleRestart()
{
    QDateTime when(QDate::currentDate(), configuredRestartTime());
    if (when <= QDateTime::currentDateTime()) {
        when = when.addDays(1);
    }
    auto msg = QDBusMessage::createMethodCall(u"org.freedesktop.login1"_s, u"/org/freedesktop/login1"_s,
                                              u"org.freedesktop.login1.Manager"_s, u"ScheduleShutdown"_s);
    msg << u"reboot"_s << static_cast<qulonglong>(when.toMSecsSinceEpoch()) * 1000;
    QDBusConnection::systemBus().asyncCall(msg);

    auto confirmation = new KNotification(u"restartScheduled"_s);
    confirmation->setComponentName(u"bootcupdates"_s);
    confirmation->setTitle(i18n("Restart scheduled"));
    confirmation->setText(i18n("The system will restart at %1 to apply the update.",
                               when.toString(u"ddd HH:mm"_s)));
    confirmation->setIconName(u"clock"_s);
    confirmation->sendEvent();
}
