#pragma once

#include <QFileSystemWatcher>
#include <QObject>

// Watches for a staged deployment and raises a notification with restart
// actions. Closing the notification dismisses it. No snooze state.
class Notifier : public QObject
{
    Q_OBJECT

public:
    explicit Notifier(QObject *parent = nullptr);

private:
    void evaluate();
    void notifyStaged();
    void restartNow();
    void scheduleRestart();

    QFileSystemWatcher m_watcher;
    bool m_notified = false;
};
