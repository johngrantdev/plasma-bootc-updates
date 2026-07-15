#include "kcm.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <KLocalizedString>
#include <KPluginFactory>

#include "packagediff.h"

using namespace Qt::StringLiterals;

namespace
{
const auto SERVICE = u"io.github.jayelg.BootcUpdates1"_s;
const auto PATH = u"/io/github/jayelg/BootcUpdates1"_s;
const auto IFACE = u"io.github.jayelg.BootcUpdates1"_s;
const auto LOGIN1_SERVICE = u"org.freedesktop.login1"_s;
const auto LOGIN1_PATH = u"/org/freedesktop/login1"_s;
const auto LOGIN1_IFACE = u"org.freedesktop.login1.Manager"_s;

QDBusMessage daemonCall(const QString &method)
{
    return QDBusMessage::createMethodCall(SERVICE, PATH, IFACE, method);
}
}

K_PLUGIN_CLASS_WITH_JSON(KCMBootcUpdates, "kcm_bootcupdates.json")

KCMBootcUpdates::KCMBootcUpdates(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
{
    setButtons(NoAdditionalButton);

    auto bus = QDBusConnection::systemBus();
    bus.connect(SERVICE, PATH, IFACE, u"CheckCompleted"_s, this, SLOT(operationFinished(bool,QString)));
    bus.connect(SERVICE, PATH, IFACE, u"StageCompleted"_s, this, SLOT(operationFinished(bool,QString)));

    refresh();
}

void KCMBootcUpdates::refresh()
{
    // This file exists exactly when a deployment is staged (no root needed).
    m_updateStaged = QFile::exists(u"/run/ostree/staged-deployment"_s);
    Q_EMIT statusChanged();

    if (m_updateStaged) {
        loadChangelog();
    } else {
        m_changelog.clear();
        m_changelogTotal = 0;
        m_changelogError.clear();
        Q_EMIT changelogChanged();
    }

    auto watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(daemonCall(u"Status"_s), 30000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QString> reply = *w;
        if (reply.isError()) {
            setMessage(i18n("Could not read system status: %1", reply.error().message()));
            return;
        }
        handleStatusJson(reply.value());
    });
}

void KCMBootcUpdates::handleStatusJson(const QString &json)
{
    const auto status = QJsonDocument::fromJson(json.toUtf8()).object()[u"status"_s].toObject();
    const auto imageInfo = [](const QJsonObject &deployment) {
        const auto image = deployment[u"image"_s].toObject();
        return qMakePair(image[u"image"_s].toObject()[u"image"_s].toString(),
                         image[u"version"_s].toString());
    };

    const auto booted = imageInfo(status[u"booted"_s].toObject());
    m_bootedImage = booted.first;
    m_bootedVersion = booted.second;

    const auto stagedObj = status[u"staged"_s].toObject();
    m_updateStaged = !stagedObj.isEmpty();
    m_stagedVersion = imageInfo(stagedObj).second;

    Q_EMIT statusChanged();
}

void KCMBootcUpdates::startOperation(const QString &method, const QString &busyMessage)
{
    setBusy(true);
    setMessage(busyMessage);
    // The call returns immediately. Completion arrives as a signal. Clear
    // busy here only if the call itself errors out.
    auto watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(daemonCall(method)), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError()) {
            setBusy(false);
            setMessage(i18n("Operation failed: %1", w->error().message()));
        }
    });
}

void KCMBootcUpdates::checkNow()
{
    startOperation(u"Check"_s, i18n("Checking for updates…"));
}

void KCMBootcUpdates::stageNow()
{
    startOperation(u"Stage"_s, i18n("Downloading and staging update…"));
}

void KCMBootcUpdates::operationFinished(bool success, const QString &output)
{
    setBusy(false);
    setMessage(success ? output : i18n("Operation failed: %1", output));
    refresh();
}

void KCMBootcUpdates::restartNow()
{
    auto msg = QDBusMessage::createMethodCall(LOGIN1_SERVICE, LOGIN1_PATH, LOGIN1_IFACE, u"Reboot"_s);
    msg << true; // interactive: let polkit prompt if needed
    QDBusConnection::systemBus().asyncCall(msg);
}

void KCMBootcUpdates::scheduleRestart(int hour, int minute)
{
    QDateTime when(QDate::currentDate(), QTime(hour, minute));
    if (when <= QDateTime::currentDateTime()) {
        when = when.addDays(1);
    }
    auto msg = QDBusMessage::createMethodCall(LOGIN1_SERVICE, LOGIN1_PATH, LOGIN1_IFACE, u"ScheduleShutdown"_s);
    msg << u"reboot"_s << static_cast<qulonglong>(when.toMSecsSinceEpoch()) * 1000;
    QDBusConnection::systemBus().asyncCall(msg);
    setMessage(i18n("Restart scheduled for %1", when.toString(u"ddd hh:mm"_s)));
}

void KCMBootcUpdates::cancelScheduledRestart()
{
    QDBusConnection::systemBus().asyncCall(
        QDBusMessage::createMethodCall(LOGIN1_SERVICE, LOGIN1_PATH, LOGIN1_IFACE, u"CancelScheduledShutdown"_s));
    setMessage(i18n("Scheduled restart cancelled"));
}

void KCMBootcUpdates::loadChangelog()
{
    if (m_changelogLoading) {
        return;
    }
    m_changelogLoading = true;
    m_changelogError.clear();
    Q_EMIT changelogChanged();

    auto diff = new PackageDiff(this);
    connect(diff, &PackageDiff::finished, this, [this, diff](const QList<PackageChange> &changes) {
        diff->deleteLater();
        m_changelog.clear();
        m_changelogTotal = changes.size();

        // changes arrive sorted by group, so fold consecutive runs into one
        // section each. QML renders each section as a collapsible header.
        QVariantList packages;
        QString group;
        auto flush = [&] {
            if (packages.isEmpty()) {
                return;
            }
            m_changelog.append(QVariantMap{
                {u"group"_s, group},
                {u"count"_s, packages.size()},
                {u"packages"_s, packages},
            });
            packages.clear();
        };
        for (const PackageChange &change : changes) {
            if (change.group != group) {
                flush();
                group = change.group;
            }
            packages.append(QVariantMap{
                {u"name"_s, change.name},
                {u"from"_s, change.from},
                {u"to"_s, change.to},
                {u"kind"_s, change.kind},
                {u"source"_s, change.source},
            });
        }
        flush();

        m_changelogLoading = false;
        Q_EMIT changelogChanged();
    });
    connect(diff, &PackageDiff::failed, this, [this, diff](const QString &error) {
        diff->deleteLater();
        m_changelog.clear();
        m_changelogTotal = 0;
        m_changelogError = error;
        m_changelogLoading = false;
        Q_EMIT changelogChanged();
    });
    diff->start();
}

void KCMBootcUpdates::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        Q_EMIT busyChanged();
    }
}

void KCMBootcUpdates::setMessage(const QString &message)
{
    m_lastMessage = message;
    Q_EMIT lastMessageChanged();
}

#include "kcm.moc"
