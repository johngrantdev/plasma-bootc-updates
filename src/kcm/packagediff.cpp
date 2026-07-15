#include "packagediff.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QtConcurrent>

#include <KLocalizedString>

using namespace Qt::StringLiterals;

namespace
{
struct PackageInfo {
    QStringList versions; // a name can appear more than once (e.g. kernel)
    QString vendor;
};
using PackageSet = QMap<QString, PackageInfo>;

// package name -> falcos build group, read from the staged image. Empty
// when the image ships no manifest, in which case we fall back to vendor.
using GroupMap = QHash<QString, QString>;

struct Deployments {
    QString booted;
    QString staged;
};

// Turns a raw rpm vendor into a short label for display and fallback
// grouping. Base-OS agnostic: the base is whatever vendor dominates the
// image (baseVendor), shown with the os-release label (e.g. "Fedora 44" or
// "Bazzite 41"). Vendorless packages are built by the image itself, so they
// get the image's own name (e.g. "falcos"). Everything else keeps its
// vendor string, with a light cleanup for Fedora Copr that no-ops elsewhere.
QString friendlySource(const QString &vendor, const QString &imageName,
                       const QString &baseVendor, const QString &baseLabel)
{
    if (vendor.isEmpty() || vendor == u"(none)"_s) {
        return imageName.isEmpty() ? i18n("Local") : imageName;
    }
    if (!baseVendor.isEmpty() && vendor == baseVendor) {
        return baseLabel;
    }
    if (vendor.startsWith(u"Fedora Copr - user "_s)) {
        return i18n("Copr: %1", vendor.mid(QStringLiteral("Fedora Copr - user ").size()));
    }
    return vendor;
}

QString unquote(const QString &value)
{
    QString v = value.trimmed();
    if (v.size() >= 2 && v.startsWith(u'"') && v.endsWith(u'"')) {
        v = v.mid(1, v.size() - 2);
    }
    return v;
}

// Reads the distro name and version from a deployment's os-release, e.g.
// "Fedora 44". Empty if os-release is missing or has no version.
QString osVersionFor(const QString &deployPath)
{
    QFile file(deployPath + u"/usr/lib/os-release"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        file.setFileName(deployPath + u"/etc/os-release"_s);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
    }
    QString name, versionId;
    const auto lines = QString::fromUtf8(file.readAll()).split(u'\n');
    for (const QString &line : lines) {
        if (line.startsWith(u"NAME="_s)) {
            name = unquote(line.section(u'=', 1));
        } else if (line.startsWith(u"VERSION_ID="_s)) {
            versionId = unquote(line.section(u'=', 1));
        }
    }
    if (versionId.isEmpty()) {
        return {};
    }
    const QString distro = name.isEmpty() ? u"Fedora"_s : name.section(u' ', 0, 0);
    return distro + u' ' + versionId;
}

// Reads the image name from a deployment's .origin, e.g.
// ghcr.io/jayelg/falcos-desktop:latest -> "falcos", dropping the flavor.
QString imageNameFor(const QString &deployPath)
{
    QFile file(deployPath + u".origin"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QString ref;
    const auto lines = QString::fromUtf8(file.readAll()).split(u'\n');
    for (const QString &line : lines) {
        if (line.startsWith(u"container-image-reference="_s)) {
            ref = line.section(u'=', 1);
            break;
        }
    }
    // Last path component, minus any digest or tag.
    QString name = ref.section(u'/', -1).section(u'@', 0, 0).section(u':', 0, 0);

    static const QStringList flavors = {u"desktop"_s, u"laptop"_s, u"server"_s, u"main"_s};
    const int dash = name.lastIndexOf(u'-');
    if (dash > 0 && flavors.contains(name.mid(dash + 1))) {
        name = name.left(dash);
    }
    return name;
}

QString runOrThrow(const QString &program, const QStringList &args, QString *error)
{
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForFinished(30000)) {
        *error = i18n("%1 did not finish in time", program);
        return {};
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        *error = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (error->isEmpty()) {
            *error = i18n("%1 failed", program);
        }
        return {};
    }
    return QString::fromUtf8(proc.readAllStandardOutput());
}

// "* default <csum>.<serial>" is booted, "(staged)" marks the staged one.
Deployments findDeployments(QString *error)
{
    const QString output = runOrThrow(u"ostree"_s, {u"admin"_s, u"status"_s}, error);
    if (!error->isEmpty()) {
        return {};
    }

    static const QRegularExpression line(uR"(^(\*)?\s*(\S+)\s+([0-9a-f]{64}\.\d+)(.*)$)"_s);
    Deployments deployments;
    const auto lines = output.split(u'\n');
    for (const QString &l : lines) {
        const auto match = line.match(l);
        if (!match.hasMatch()) {
            continue;
        }
        const QString path = u"/ostree/deploy/%1/deploy/%2"_s.arg(match.captured(2), match.captured(3));
        if (match.captured(4).contains(u"(staged)"_s)) {
            deployments.staged = path;
        } else if (match.captured(1) == u"*"_s) {
            deployments.booted = path;
        }
    }

    if (deployments.staged.isEmpty()) {
        *error = i18n("No staged deployment found");
    } else if (deployments.booted.isEmpty()) {
        *error = i18n("Could not identify the booted deployment");
    }
    return deployments;
}

PackageSet readPackages(const QString &root, QString *error)
{
    const QString output = runOrThrow(
        u"rpm"_s, {u"-qa"_s, u"--root"_s, root, u"--qf"_s, u"%{NAME}\t%{EVR}\t%{VENDOR}\n"_s}, error);
    if (!error->isEmpty()) {
        return {};
    }

    PackageSet packages;
    const auto lines = output.split(u'\n', Qt::SkipEmptyParts);
    for (const QString &l : lines) {
        const auto parts = l.split(u'\t');
        if (parts.size() != 3) {
            continue;
        }
        // Not a real package, and it has many entries sharing the one name.
        if (parts[0] == u"gpg-pubkey"_s) {
            continue;
        }
        auto &info = packages[parts[0]];
        info.versions.append(parts[1]);
        info.vendor = parts[2] == u"(none)"_s ? QString() : parts[2];
    }
    for (auto it = packages.begin(); it != packages.end(); ++it) {
        it->versions.sort();
    }
    return packages;
}

// falcos bakes a { group: [package, ...] } map into the image. We read it
// from the staged tree and invert it to package -> group. Absent is fine.
GroupMap readGroupMap(const QString &root)
{
    QFile file(root + u"/usr/share/falcos/package-groups.json"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return {};
    }
    GroupMap map;
    const auto groups = doc.object();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const auto packages = it.value().toArray();
        for (const auto &pkg : packages) {
            map.insert(pkg.toString(), it.key());
        }
    }
    return map;
}

// Sort key: falcos groups keep their manifest order via a leading index;
// the catch-all base/Fedora buckets always sink to the bottom.
QString groupSortKey(const QString &group, bool isBase)
{
    return (isBase ? u"2_"_s : u"1_"_s) + group;
}
}

PackageDiff::PackageDiff(QObject *parent)
    : QObject(parent)
{
}

void PackageDiff::start()
{
    auto work = [] {
        QString error;
        const Deployments deployments = findDeployments(&error);
        if (!error.isEmpty()) {
            return std::make_pair(QList<PackageChange>(), error);
        }

        const PackageSet booted = readPackages(deployments.booted, &error);
        if (!error.isEmpty()) {
            return std::make_pair(QList<PackageChange>(), error);
        }
        const PackageSet staged = readPackages(deployments.staged, &error);
        if (!error.isEmpty()) {
            return std::make_pair(QList<PackageChange>(), error);
        }
        const GroupMap groups = readGroupMap(deployments.staged);
        const bool haveManifest = !groups.isEmpty();
        const QString imageName = imageNameFor(deployments.staged);

        // The base OS is whatever vendor supplies the most packages. Label it
        // from os-release, or "Base system" if the image has no os-release.
        QHash<QString, int> vendorCounts;
        for (const auto &info : staged) {
            if (!info.vendor.isEmpty()) {
                ++vendorCounts[info.vendor];
            }
        }
        QString baseVendor;
        int mostSeen = 0;
        for (auto it = vendorCounts.constBegin(); it != vendorCounts.constEnd(); ++it) {
            if (it.value() > mostSeen) {
                mostSeen = it.value();
                baseVendor = it.key();
            }
        }
        const QString osLabel = osVersionFor(deployments.staged);
        const QString baseLabel = osLabel.isEmpty() ? i18n("Base system") : osLabel;

        QList<PackageChange> changes;
        auto names = booted.keys();
        names.append(staged.keys());
        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());

        for (const QString &name : std::as_const(names)) {
            const bool inBooted = booted.contains(name);
            const bool inStaged = staged.contains(name);
            const QString from = booted.value(name).versions.join(u", "_s);
            const QString to = staged.value(name).versions.join(u", "_s);

            PackageChange change;
            if (inBooted && inStaged) {
                if (from == to) {
                    continue;
                }
                change = {name, from, to, PackageChange::Changed, {}, {}};
            } else if (inStaged) {
                change = {name, {}, to, PackageChange::Added, {}, {}};
            } else {
                change = {name, from, {}, PackageChange::Removed, {}, {}};
            }

            // Prefer the staged vendor; a removed package only exists in booted.
            const QString vendor =
                inStaged ? staged.value(name).vendor : booted.value(name).vendor;
            change.source = friendlySource(vendor, imageName, baseVendor, baseLabel);

            // Top level is the falcos build group when the manifest knows the
            // package. Anything else (transitive deps) is the base OS.
            // Without a manifest we group by source instead.
            if (haveManifest) {
                change.group = groups.value(name, baseLabel);
            } else {
                change.group = change.source;
            }
            changes.append(change);
        }

        const auto isBase = [&](const QString &g) {
            return g == baseLabel;
        };
        std::sort(changes.begin(), changes.end(), [&](const PackageChange &a, const PackageChange &b) {
            const QString ka = groupSortKey(a.group, isBase(a.group));
            const QString kb = groupSortKey(b.group, isBase(b.group));
            return ka != kb ? ka < kb : a.name < b.name;
        });
        return std::make_pair(changes, QString());
    };

    auto watcher = new QFutureWatcher<std::pair<QList<PackageChange>, QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        watcher->deleteLater();
        const auto [changes, error] = watcher->result();
        if (error.isEmpty()) {
            Q_EMIT finished(changes);
        } else {
            Q_EMIT failed(error);
        }
    });
    watcher->setFuture(QtConcurrent::run(work));
}
