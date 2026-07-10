#include "updater.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QMetaObject>

using namespace Qt::StringLiterals;

static const auto BOOTC = u"/usr/bin/bootc"_s;

// polkit CheckAuthorization subject: (sa{sv})
struct PolkitSubject {
    QString kind;
    QVariantMap details;
};
Q_DECLARE_METATYPE(PolkitSubject)

QDBusArgument &operator<<(QDBusArgument &arg, const PolkitSubject &subject)
{
    arg.beginStructure();
    arg << subject.kind << subject.details;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, PolkitSubject &subject)
{
    arg.beginStructure();
    arg >> subject.kind >> subject.details;
    arg.endStructure();
    return arg;
}

Updater::Updater(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<PolkitSubject>();
    qDBusRegisterMetaType<QMap<QString, QString>>();
    // Exit when idle. The bus reactivates us on demand.
    m_idleTimer.setInterval(10 * 60 * 1000);
    m_idleTimer.setSingleShot(true);
    connect(&m_idleTimer, &QTimer::timeout, this, [this] {
        if (!m_proc) {
            QCoreApplication::quit();
        }
    });
    m_idleTimer.start();
}

bool Updater::authorize(const QString &actionId)
{
    m_idleTimer.start();
    // Talk to polkitd directly rather than via a cached proxy.
    auto msg = QDBusMessage::createMethodCall(u"org.freedesktop.PolicyKit1"_s,
                                              u"/org/freedesktop/PolicyKit1/Authority"_s,
                                              u"org.freedesktop.PolicyKit1.Authority"_s,
                                              u"CheckAuthorization"_s);
    const PolkitSubject subject{u"system-bus-name"_s, {{u"name"_s, message().service()}}};
    const quint32 allowUserInteraction = 1;
    msg << QVariant::fromValue(subject) << actionId
        << QVariant::fromValue(QMap<QString, QString>()) << allowUserInteraction << QString();

    const QDBusMessage reply = QDBusConnection::systemBus().call(msg, QDBus::Block, 60000);
    bool authorized = false;
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        // Result is (bba{ss}), we only need the first bool.
        const auto result = reply.arguments().first().value<QDBusArgument>();
        result.beginStructure();
        result >> authorized;
    }
    if (!authorized) {
        sendErrorReply(QDBusError::AccessDenied, u"Not authorized for %1"_s.arg(actionId));
        return false;
    }
    return true;
}

QString Updater::Status()
{
    if (!authorize(u"io.github.jayelg.bootcupdates.status"_s)) {
        return {};
    }
    QProcess proc;
    proc.start(BOOTC, {u"status"_s, u"--json"_s});
    if (!proc.waitForFinished(15000) || proc.exitCode() != 0) {
        sendErrorReply(QDBusError::Failed, QString::fromUtf8(proc.readAllStandardError()));
        return {};
    }
    return QString::fromUtf8(proc.readAllStandardOutput());
}

void Updater::Check()
{
    if (!authorize(u"io.github.jayelg.bootcupdates.check"_s)) {
        return;
    }
    runAsync({u"upgrade"_s, u"--check"_s}, "CheckCompleted");
}

void Updater::Stage()
{
    if (!authorize(u"io.github.jayelg.bootcupdates.stage"_s)) {
        return;
    }
    // No --apply: stage only, never reboot.
    runAsync({u"upgrade"_s}, "StageCompleted");
}

bool Updater::Busy()
{
    m_idleTimer.start();
    return m_proc != nullptr;
}

void Updater::runAsync(const QStringList &args, const char *signalName)
{
    if (m_proc) {
        sendErrorReply(QDBusError::Failed, u"An operation is already in progress"_s);
        return;
    }
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::finished, this,
            [this, signalName](int exitCode, QProcess::ExitStatus exitStatus) {
                const QString output = QString::fromUtf8(m_proc->readAll()).trimmed();
                const bool ok = exitStatus == QProcess::NormalExit && exitCode == 0;
                m_proc->deleteLater();
                m_proc = nullptr;
                m_idleTimer.start();
                QMetaObject::invokeMethod(this, signalName, Q_ARG(bool, ok), Q_ARG(QString, output));
            });
    m_proc->start(BOOTC, args);
}
