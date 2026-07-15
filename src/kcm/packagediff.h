#pragma once

#include <QList>
#include <QObject>
#include <QString>

struct PackageChange {
    enum Kind { Changed, Added, Removed };

    QString name;
    QString from; // empty when Added
    QString to;   // empty when Removed
    Kind kind = Changed;
    QString group;  // build-group from the falcos manifest, or the source
    QString source; // friendly vendor, e.g. "Fedora", "Copr: secureblue"
};

// Diffs the booted and staged deployments by reading each one's rpmdb.
// Both live on disk and are world readable, so this needs no root and no
// network. Runs off the GUI thread; rpm takes a moment over 2000 packages.
class PackageDiff : public QObject
{
    Q_OBJECT

public:
    explicit PackageDiff(QObject *parent = nullptr);

    void start();

Q_SIGNALS:
    void finished(const QList<PackageChange> &changes);
    void failed(const QString &error);
};
