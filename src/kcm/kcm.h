#pragma once

#include <QVariantList>

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
    Q_PROPERTY(QVariantList changelog READ changelog NOTIFY changelogChanged)
    Q_PROPERTY(int changelogTotal READ changelogTotal NOTIFY changelogChanged)
    Q_PROPERTY(int securityCount READ securityCount NOTIFY changelogChanged)
    Q_PROPERTY(bool changelogLoading READ changelogLoading NOTIFY changelogChanged)
    Q_PROPERTY(QString changelogError READ changelogError NOTIFY changelogChanged)

public:
    explicit KCMBootcUpdates(QObject *parent, const KPluginMetaData &metaData);

    QString bootedVersion() const { return m_bootedVersion; }
    QString bootedImage() const { return m_bootedImage; }
    QString stagedVersion() const { return m_stagedVersion; }
    bool updateStaged() const { return m_updateStaged; }
    bool busy() const { return m_busy; }
    QString lastMessage() const { return m_lastMessage; }
    QVariantList changelog() const { return m_changelog; }
    int changelogTotal() const { return m_changelogTotal; }
    int securityCount() const { return m_securityCount; }
    bool changelogLoading() const { return m_changelogLoading; }
    QString changelogError() const { return m_changelogError; }

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
    void changelogChanged();

private Q_SLOTS:
    // handles the daemon's CheckCompleted/StageCompleted signals
    void operationFinished(bool success, const QString &output);

private:
    void startOperation(const QString &method, const QString &busyMessage);
    void setBusy(bool busy);
    void setMessage(const QString &message);
    void handleStatusJson(const QString &json);
    void loadChangelog();

    QString m_bootedVersion;
    QString m_bootedImage;
    QString m_stagedVersion;
    bool m_updateStaged = false;
    bool m_busy = false;
    QString m_lastMessage;
    QVariantList m_changelog;
    int m_changelogTotal = 0;
    int m_securityCount = 0;
    bool m_changelogLoading = false;
    QString m_changelogError;
};
