#pragma once

#include <KQuickConfigModule>

class KCMBootcUpdates : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(QString bootedVersion READ bootedVersion NOTIFY statusChanged)
    Q_PROPERTY(QString bootedImage READ bootedImage NOTIFY statusChanged)
    Q_PROPERTY(QString stagedVersion READ stagedVersion NOTIFY statusChanged)
    Q_PROPERTY(bool updateStaged READ updateStaged NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)

public:
    explicit KCMBootcUpdates(QObject *parent, const KPluginMetaData &metaData);

    QString bootedVersion() const { return m_bootedVersion; }
    QString bootedImage() const { return m_bootedImage; }
    QString stagedVersion() const { return m_stagedVersion; }
    bool updateStaged() const { return m_updateStaged; }
    bool busy() const { return m_busy; }
    QString lastMessage() const { return m_lastMessage; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void checkNow();
    Q_INVOKABLE void stageNow();
    Q_INVOKABLE void restartNow();
    // schedules the next occurrence of hour:minute via logind
    Q_INVOKABLE void scheduleRestart(int hour, int minute);
    Q_INVOKABLE void cancelScheduledRestart();

Q_SIGNALS:
    void statusChanged();
    void busyChanged();
    void lastMessageChanged();

private Q_SLOTS:
    // handles the daemon's CheckCompleted/StageCompleted signals
    void operationFinished(bool success, const QString &output);

private:
    void startOperation(const QString &method, const QString &busyMessage);
    void setBusy(bool busy);
    void setMessage(const QString &message);
    void handleStatusJson(const QString &json);

    QString m_bootedVersion;
    QString m_bootedImage;
    QString m_stagedVersion;
    bool m_updateStaged = false;
    bool m_busy = false;
    QString m_lastMessage;
};
