#include "notifier.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDateTime>
#include <QFile>
#include <QProcess>
#include <QSet>
#include <QStringList>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KNotification>
#include <KSharedConfig>

#include "packagediff.h"

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
        startStagedNotification();
    } else if (!staged) {
        m_notified = false;
    }
}

// Run the same diff the KCM shows, then summarise it in the notification.
// Falls back to a plain message if the diff cannot be read.
void Notifier::startStagedNotification()
{
    auto diff = new PackageDiff(this);
    connect(diff, &PackageDiff::finished, this, [this, diff](const QList<PackageChange> &changes) {
        diff->deleteLater();

        QSet<QString> cves;
        for (const PackageChange &change : changes) {
            cves.unite(QSet<QString>(change.cves.begin(), change.cves.end()));
        }

        QString summary = i18np("%1 package updated", "%1 packages updated", changes.size());
        if (!cves.isEmpty()) {
            summary += u'\n' + i18np("%1 vulnerability fixed", "%1 vulnerabilities fixed", cves.size());
        }
        showStagedNotification(summary);
    });
    connect(diff, &PackageDiff::failed, this, [this, diff](const QString &) {
        diff->deleteLater();
        showStagedNotification(i18n("An update has been staged and will apply on the next restart."));
    });
    diff->start();
}

void Notifier::showStagedNotification(const QString &summary)
{
    auto notification = new KNotification(u"updateStaged"_s);
    notification->setComponentName(u"bootcupdates"_s);
    notification->setTitle(i18n("System update ready"));
    notification->setText(summary);
    notification->setIconName(u"system-software-update"_s);
    notification->setFlags(KNotification::Persistent);

    auto restart = notification->addAction(i18n("Restart Now"));
    connect(restart, &KNotificationAction::activated, this, &Notifier::restartNow);

    const QTime when = configuredRestartTime();
    auto schedule = notification->addAction(i18n("Restart at %1", when.toString(u"HH:mm"_s)));
    connect(schedule, &KNotificationAction::activated, this, &Notifier::scheduleRestart);

    auto openSettings = [] {
        QProcess::startDetached(u"systemsettings"_s, {u"kcm_bootcupdates"_s});
    };
    // Explicit button plus body-click, since the default action alone is
    // easy to miss.
    auto review = notification->addAction(i18n("Review"));
    connect(review, &KNotificationAction::activated, this, openSettings);
    auto openDefault = notification->addDefaultAction(i18n("Review"));
    connect(openDefault, &KNotificationAction::activated, this, openSettings);

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
