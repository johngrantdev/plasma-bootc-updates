#pragma once

#include <QDBusContext>
#include <QObject>
#include <QProcess>
#include <QTimer>

// Privileged wrapper around the bootc CLI. Each call is gated by its own
// polkit action.
class Updater : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.jayelg.BootcUpdates1")

public:
    explicit Updater(QObject *parent = nullptr);

public Q_SLOTS:
    // bootc status --json
    Q_SCRIPTABLE QString Status();
    // bootc upgrade --check, result via CheckCompleted
    Q_SCRIPTABLE void Check();
    // bootc upgrade (stage only, never --apply), result via StageCompleted
    Q_SCRIPTABLE void Stage();
    Q_SCRIPTABLE bool Busy();

Q_SIGNALS:
    Q_SCRIPTABLE void CheckCompleted(bool success, const QString &output);
    Q_SCRIPTABLE void StageCompleted(bool success, const QString &output);

private:
    bool authorize(const QString &actionId);
    void runAsync(const QStringList &args, const char *signalName);

    QProcess *m_proc = nullptr;
    QTimer m_idleTimer;
};
