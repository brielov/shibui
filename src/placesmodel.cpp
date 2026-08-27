#include "placesmodel.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QtConcurrentRun>

#include <algorithm>
#include <unistd.h>
#include <utility>

namespace {
QString firstMountPoint(const QJsonObject &object)
{
    const QJsonValue mountpoints = object.value(QStringLiteral("mountpoints"));
    if (mountpoints.isArray()) {
        for (const QJsonValue &value : mountpoints.toArray()) {
            if (value.isString() && !value.toString().isEmpty())
                return value.toString();
        }
    }
    return object.value(QStringLiteral("mountpoint")).toString();
}

void collectBlockDevices(const QJsonArray &nodes, bool parentEjectable,
                         const QString &parentDisk, QVector<PlaceEntry> &entries)
{
    for (const QJsonValue &value : nodes) {
        const QJsonObject object = value.toObject();
        const QString type = object.value(QStringLiteral("type")).toString();
        const QString devicePath = object.value(QStringLiteral("path")).toString();
        const bool ejectable = parentEjectable
            || object.value(QStringLiteral("rm")).toBool()
            || object.value(QStringLiteral("rm")).toInt() != 0
            || object.value(QStringLiteral("hotplug")).toBool()
            || object.value(QStringLiteral("hotplug")).toInt() != 0;
        const QString diskPath = type == QStringLiteral("disk") ? devicePath : parentDisk;
        const QString mountPath = firstMountPoint(object);
        const QString filesystem = object.value(QStringLiteral("fstype")).toString();
        const bool userMount = mountPath.startsWith(QStringLiteral("/run/media/"))
            || mountPath.startsWith(QStringLiteral("/media/"))
            || mountPath.startsWith(QStringLiteral("/mnt/"));
        if (!devicePath.isEmpty() && !filesystem.isEmpty() && (ejectable || userMount)) {
            QString label = object.value(QStringLiteral("label")).toString();
            if (label.isEmpty())
                label = object.value(QStringLiteral("name")).toString();
            if (label.isEmpty())
                label = devicePath;
            entries.append({label, mountPath.isEmpty() ? QString() : QDir::cleanPath(mountPath),
                            mountPath.isEmpty() ? QStringLiteral("device")
                                                : QStringLiteral("volume"),
                            devicePath, diskPath.isEmpty() ? devicePath : diskPath,
                            !mountPath.isEmpty(), ejectable, {}});
        }
        collectBlockDevices(object.value(QStringLiteral("children")).toArray(),
                            ejectable, diskPath, entries);
    }
}

QHash<QString, QString> mountParameters(const QString &name)
{
    QHash<QString, QString> parameters;
    const QString encoded = name.section(QLatin1Char(':'), 1);
    for (const QString &part : encoded.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const qsizetype equals = part.indexOf(QLatin1Char('='));
        if (equals > 0)
            parameters.insert(part.left(equals), QUrl::fromPercentEncoding(
                                  part.mid(equals + 1).toUtf8()));
    }
    return parameters;
}

QString uriForGvfsMount(const QString &name)
{
    const QString type = name.section(QLatin1Char(':'), 0, 0);
    const QHash<QString, QString> parameters = mountParameters(name);
    const QString host = parameters.value(QStringLiteral("server"),
                                           parameters.value(QStringLiteral("host")));
    if (host.isEmpty())
        return {};
    QUrl url;
    if (type == QStringLiteral("smb-share") || type == QStringLiteral("smb-server")) {
        url.setScheme(QStringLiteral("smb"));
        const QString share = parameters.value(QStringLiteral("share"));
        if (!share.isEmpty())
            url.setPath(QLatin1Char('/') + share);
    } else if (type == QStringLiteral("sftp")) {
        url.setScheme(QStringLiteral("sftp"));
    } else if (type.startsWith(QStringLiteral("dav"))) {
        url.setScheme(parameters.value(QStringLiteral("ssl")) == QStringLiteral("true")
                          ? QStringLiteral("davs") : QStringLiteral("dav"));
        url.setPath(parameters.value(QStringLiteral("prefix")));
    } else if (type == QStringLiteral("nfs")) {
        url.setScheme(QStringLiteral("nfs"));
        url.setPath(parameters.value(QStringLiteral("path")));
    } else {
        return {};
    }
    url.setHost(host);
    url.setUserName(parameters.value(QStringLiteral("user")));
    if (!parameters.value(QStringLiteral("port")).isEmpty())
        url.setPort(parameters.value(QStringLiteral("port")).toInt());
    return url.toString(QUrl::FullyEncoded);
}

QVector<PlaceEntry> networkMounts()
{
    QVector<PlaceEntry> entries;
    const QDir directory(QStringLiteral("/run/user/%1/gvfs").arg(getuid()));
    for (const QFileInfo &info : directory.entryInfoList(
             QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)) {
        const QString uri = uriForGvfsMount(info.fileName());
        if (uri.isEmpty())
            continue;
        const QUrl url(uri);
        QString label = url.host();
        const QString leaf = url.path().section(QLatin1Char('/'), -1);
        if (!leaf.isEmpty())
            label = leaf + QStringLiteral(" on ") + label;
        entries.append({label, info.absoluteFilePath(), QStringLiteral("network"),
                        {}, {}, true, false, uri});
    }
    return entries;
}

QString networkLabel(const QString &uri);

QVector<PlaceEntry> discoveredNetworkPlaces()
{
    QVector<PlaceEntry> entries;
    if (qEnvironmentVariableIsSet("SHIBUI_SKIP_DESKTOP_QUERIES"))
        return entries;
    const QString gio = QStandardPaths::findExecutable(QStringLiteral("gio"));
    if (gio.isEmpty())
        return entries;

    QProcess process;
    process.start(gio,
                  {QStringLiteral("list"), QStringLiteral("-l"), QStringLiteral("-u"),
                   QStringLiteral("-a"),
                   QStringLiteral("standard::display-name,standard::target-uri"),
                   QStringLiteral("network:///")}, QIODevice::ReadOnly);
    if (!process.waitForFinished(3000) || process.exitCode() != 0)
        return entries;

    QSet<QString> uris;
    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        QString uri = line.section(QLatin1Char('\t'), 0, 0).trimmed();
        const QString targetMarker = QStringLiteral("standard::target-uri=");
        const qsizetype targetStart = line.indexOf(targetMarker);
        if (targetStart >= 0) {
            uri = line.mid(targetStart + targetMarker.size())
                      .section(QLatin1Char(' '), 0, 0).trimmed();
        }
        const QUrl url(uri);
        const QString scheme = url.scheme().toCaseFolded();
        if (url.host().isEmpty()
            || (scheme != QStringLiteral("smb") && scheme != QStringLiteral("sftp")
                && scheme != QStringLiteral("dav") && scheme != QStringLiteral("davs")
                && scheme != QStringLiteral("nfs")) || uris.contains(uri))
            continue;

        QString label;
        const QString displayMarker = QStringLiteral("standard::display-name=");
        const qsizetype displayStart = line.indexOf(displayMarker);
        if (displayStart >= 0) {
            label = line.mid(displayStart + displayMarker.size());
            const qsizetype nextAttribute = label.indexOf(QStringLiteral(" standard::"));
            if (nextAttribute >= 0)
                label.truncate(nextAttribute);
            label = label.trimmed();
        }
        if (label.isEmpty())
            label = networkLabel(uri);
        entries.append({label, {}, QStringLiteral("network-discovery"), {}, {},
                        false, false, uri});
        uris.insert(uri);
    }
    return entries;
}

QString comparableNetworkUri(const QString &uri)
{
    QUrl url(uri);
    url.setPassword({});
    url.setScheme(url.scheme().toCaseFolded());
    url.setHost(url.host().toCaseFolded());
    return url.adjusted(QUrl::StripTrailingSlash).toString(QUrl::FullyEncoded);
}

QString networkLabel(const QString &uri)
{
    const QUrl url(uri);
    QString label = url.host();
    const QString leaf = url.path().section(QLatin1Char('/'), -1);
    if (!leaf.isEmpty())
        label = leaf + QStringLiteral(" on ") + label;
    return label.isEmpty() ? uri : label;
}
}

PlacesModel::PlacesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    QSettings settings;
    const QStringList paths = settings.value(QStringLiteral("bookmarks/paths")).toStringList();
    const QStringList labels = settings.value(QStringLiteral("bookmarks/labels")).toStringList();
    for (int index = 0; index < paths.size(); ++index) {
        const QString stored = paths.at(index);
        const QUrl url(stored);
        const bool network = !url.scheme().isEmpty()
            && url.scheme() != QStringLiteral("file");
        QString label = labels.value(index).trimmed();
        if (network) {
            if (label.isEmpty())
                label = networkLabel(stored);
            m_bookmarks.append({label, {}, QStringLiteral("bookmark"), {}, {},
                                false, false, stored});
            continue;
        }
        const QString path = QDir::cleanPath(stored);
        if (!QFileInfo(path).isDir())
            continue;
        if (label.isEmpty())
            label = QFileInfo(path).fileName();
        m_bookmarks.append({label, path, QStringLiteral("bookmark"), {}, {},
                            false, false, {}});
    }

    connect(&m_watcher, &QFutureWatcher<QVector<PlaceEntry>>::finished, this, [this] {
        beginResetModel();
        m_places = m_watcher.result();
        m_discoveredNetworks.clear();
        for (const PlaceEntry &place : std::as_const(m_places)) {
            if (place.kind == QStringLiteral("network-discovery"))
                m_discoveredNetworks.append(place);
        }
        endResetModel();
        emit countChanged();
        emit refreshed();
        if (m_refreshPending)
            refresh();
    });

    m_refreshTimer.setInterval(10000);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this] {
        if (!m_watcher.isRunning())
            refresh();
    });
    m_refreshTimer.start();
    refresh();
}

PlacesModel::~PlacesModel()
{
    if (m_deviceProcess) {
        m_deviceProcess->kill();
        m_deviceProcess->waitForFinished(1000);
    }
    m_watcher.waitForFinished();
}

int PlacesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_places.size();
}

QVariant PlacesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_places.size())
        return {};
    const PlaceEntry &place = m_places.at(index.row());
    switch (role) {
    case LabelRole:
        return place.label;
    case PathRole:
        return place.path;
    case KindRole:
        return place.kind;
    case TrashRole:
        return place.kind == QStringLiteral("trash");
    case VolumeRole:
        return place.kind == QStringLiteral("volume");
    case BookmarkRole:
        return place.kind == QStringLiteral("bookmark");
    case DevicePathRole:
        return place.devicePath;
    case MountedRole:
        return place.mounted;
    case EjectableRole:
        return place.ejectable;
    case NetworkRole:
        return !place.networkUri.isEmpty();
    case NetworkUriRole:
        return place.networkUri;
    case RecentRole:
        return place.kind == QStringLiteral("recent");
    case SectionRole:
        if (place.kind == QStringLiteral("bookmark"))
            return QStringLiteral("BOOKMARKS");
        if (place.kind == QStringLiteral("volume")
            || place.kind == QStringLiteral("device"))
            return QStringLiteral("DEVICES");
        if (place.kind.startsWith(QStringLiteral("network")))
            return QStringLiteral("NETWORK");
        return QStringLiteral("PLACES");
    default:
        return {};
    }
}

QHash<int, QByteArray> PlacesModel::roleNames() const
{
    return {
        {LabelRole, "placeLabel"},
        {PathRole, "placePath"},
        {KindRole, "placeKind"},
        {TrashRole, "placeIsTrash"},
        {VolumeRole, "placeIsVolume"},
        {BookmarkRole, "placeIsBookmark"},
        {DevicePathRole, "placeDevicePath"},
        {MountedRole, "placeIsMounted"},
        {EjectableRole, "placeIsEjectable"},
        {NetworkRole, "placeIsNetwork"},
        {NetworkUriRole, "placeNetworkUri"},
        {RecentRole, "placeIsRecent"},
        {SectionRole, "placeSection"},
    };
}

void PlacesModel::refresh()
{
    if (m_watcher.isRunning()) {
        m_refreshPending = true;
        return;
    }
    m_refreshPending = false;
    const QVector<PlaceEntry> bookmarks = m_bookmarks;
    const QStringList recentNetworkUris =
        QSettings().value(QStringLiteral("network/recent")).toStringList();
    const QVector<PlaceEntry> knownDiscoveries = m_discoveredNetworks;
    const bool discoverNetworks = !m_discoveryAge.isValid()
        || m_discoveryAge.elapsed() >= 30000;
    if (discoverNetworks)
        m_discoveryAge.restart();
    m_watcher.setFuture(QtConcurrent::run(
        [bookmarks, recentNetworkUris, knownDiscoveries, discoverNetworks] {
        return collectPlaces(bookmarks, recentNetworkUris, knownDiscoveries,
                             discoverNetworks);
    }));
}

QString PlacesModel::pathAt(int row) const
{
    return row >= 0 && row < m_places.size() ? m_places.at(row).path : QString();
}

bool PlacesModel::isTrashAt(int row) const
{
    return row >= 0 && row < m_places.size()
        && m_places.at(row).kind == QStringLiteral("trash");
}

bool PlacesModel::isRecentAt(int row) const
{
    return row >= 0 && row < m_places.size()
        && m_places.at(row).kind == QStringLiteral("recent");
}

bool PlacesModel::isBookmarkAt(int row) const
{
    return row >= 0 && row < m_places.size()
        && m_places.at(row).kind == QStringLiteral("bookmark");
}

QString PlacesModel::labelAt(int row) const
{
    return row >= 0 && row < m_places.size() ? m_places.at(row).label : QString();
}

bool PlacesModel::deviceActionActive() const
{
    return m_deviceProcess != nullptr;
}

bool PlacesModel::isDeviceAt(int row) const
{
    return row >= 0 && row < m_places.size() && !m_places.at(row).devicePath.isEmpty();
}

bool PlacesModel::isMountedAt(int row) const
{
    return row >= 0 && row < m_places.size() && m_places.at(row).mounted;
}

bool PlacesModel::isEjectableAt(int row) const
{
    return row >= 0 && row < m_places.size() && m_places.at(row).ejectable;
}

bool PlacesModel::mountAt(int row)
{
    return startDeviceAction(row, QStringLiteral("mount"));
}

bool PlacesModel::unmountAt(int row)
{
    return startDeviceAction(row, QStringLiteral("unmount"));
}

bool PlacesModel::ejectAt(int row)
{
    return startDeviceAction(row, QStringLiteral("power-off"));
}

bool PlacesModel::isNetworkAt(int row) const
{
    return row >= 0 && row < m_places.size() && !m_places.at(row).networkUri.isEmpty();
}

QString PlacesModel::networkUriAt(int row) const
{
    return isNetworkAt(row) ? m_places.at(row).networkUri : QString();
}

QString PlacesModel::networkUriForPath(const QString &path) const
{
    const QString cleanPath = QDir::cleanPath(path);
    for (const PlaceEntry &place : m_places) {
        if (place.networkUri.isEmpty() || place.path.isEmpty())
            continue;
        if (cleanPath == place.path || cleanPath.startsWith(place.path + QLatin1Char('/')))
            return place.networkUri;
    }
    return {};
}

bool PlacesModel::addNetworkBookmark(const QString &uri, const QString &label)
{
    const QString comparable = comparableNetworkUri(uri);
    const QUrl url(comparable);
    if (!url.isValid() || comparable.isEmpty() || url.host().isEmpty())
        return false;
    for (const PlaceEntry &bookmark : std::as_const(m_bookmarks)) {
        if (!bookmark.networkUri.isEmpty()
            && comparableNetworkUri(bookmark.networkUri) == comparable)
            return false;
    }
    QString resolvedLabel = label.trimmed();
    if (resolvedLabel.isEmpty())
        resolvedLabel = networkLabel(comparable);
    m_bookmarks.append({resolvedLabel, {}, QStringLiteral("bookmark"), {}, {},
                        false, false, comparable});
    saveBookmarks();
    refresh();
    return true;
}

bool PlacesModel::startDeviceAction(int row, const QString &action)
{
    if (row < 0 || row >= m_places.size() || m_deviceProcess)
        return false;
    const PlaceEntry place = m_places.at(row);
    const QString device = action == QStringLiteral("power-off") ? place.ejectPath
                                                                  : place.devicePath;
    if (device.isEmpty() || (action == QStringLiteral("mount") && place.mounted)
        || (action == QStringLiteral("unmount") && !place.mounted)
        || (action == QStringLiteral("power-off") && !place.ejectable))
        return false;
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("udisksctl"));
    if (executable.isEmpty())
        return false;

    auto *process = new QProcess(this);
    m_deviceProcess = process;
    emit deviceActionChanged();
    connect(process, &QProcess::finished, this,
            [this, process, action](int exitCode, QProcess::ExitStatus exitStatus) {
                if (process != m_deviceProcess) {
                    process->deleteLater();
                    return;
                }
                m_deviceProcess = nullptr;
                const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
                QString output = QString::fromUtf8(success ? process->readAllStandardOutput()
                                                           : process->readAllStandardError()).trimmed();
                QString mountedPath;
                if (success && action == QStringLiteral("mount")) {
                    const qsizetype at = output.lastIndexOf(QStringLiteral(" at "));
                    if (at >= 0) {
                        mountedPath = output.mid(at + 4).trimmed();
                        if (mountedPath.endsWith(QLatin1Char('.')))
                            mountedPath.chop(1);
                    }
                }
                if (output.isEmpty())
                    output = success ? QStringLiteral("Device action completed.")
                                     : QStringLiteral("The device action failed.");
                process->deleteLater();
                emit deviceActionChanged();
                emit deviceActionFinished(success, output, mountedPath);
                refresh();
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError processError) {
                if (processError != QProcess::FailedToStart || process != m_deviceProcess)
                    return;
                m_deviceProcess = nullptr;
                const QString message = process->errorString();
                process->deleteLater();
                emit deviceActionChanged();
                emit deviceActionFinished(false, message, {});
            });
    process->start(executable, {action, QStringLiteral("-b"), device}, QIODevice::ReadOnly);
    return true;
}

bool PlacesModel::addBookmark(const QString &path, const QString &label)
{
    const QString cleanPath = QDir::cleanPath(path);
    if (!QFileInfo(cleanPath).isDir())
        return false;
    for (const PlaceEntry &place : std::as_const(m_places)) {
        if (!place.path.isEmpty() && place.path == cleanPath)
            return false;
    }
    for (const PlaceEntry &bookmark : std::as_const(m_bookmarks)) {
        if (bookmark.path == cleanPath)
            return false;
    }
    QString resolvedLabel = label.trimmed();
    if (resolvedLabel.isEmpty())
        resolvedLabel = QFileInfo(cleanPath).fileName();
    if (resolvedLabel.isEmpty())
        resolvedLabel = cleanPath;
    m_bookmarks.append({resolvedLabel, cleanPath, QStringLiteral("bookmark"), {}, {},
                        false, false, {}});
    saveBookmarks();
    refresh();
    return true;
}

bool PlacesModel::renameBookmark(int row, const QString &label)
{
    const int bookmarkIndex = bookmarkIndexForRow(row);
    const QString cleanLabel = label.trimmed();
    if (bookmarkIndex < 0 || cleanLabel.isEmpty())
        return false;
    m_bookmarks[bookmarkIndex].label = cleanLabel;
    saveBookmarks();
    refresh();
    return true;
}

bool PlacesModel::removeBookmark(int row)
{
    const int bookmarkIndex = bookmarkIndexForRow(row);
    if (bookmarkIndex < 0)
        return false;
    m_bookmarks.removeAt(bookmarkIndex);
    saveBookmarks();
    refresh();
    return true;
}

bool PlacesModel::moveBookmark(int row, int offset)
{
    const int bookmarkIndex = bookmarkIndexForRow(row);
    const int target = bookmarkIndex + offset;
    if (bookmarkIndex < 0 || target < 0 || target >= m_bookmarks.size())
        return false;
    m_bookmarks.move(bookmarkIndex, target);
    saveBookmarks();
    refresh();
    return true;
}

int PlacesModel::bookmarkIndexForRow(int row) const
{
    if (!isBookmarkAt(row))
        return -1;
    const PlaceEntry &place = m_places.at(row);
    for (int index = 0; index < m_bookmarks.size(); ++index) {
        const PlaceEntry &bookmark = m_bookmarks.at(index);
        if (!place.networkUri.isEmpty()
                ? comparableNetworkUri(bookmark.networkUri)
                    == comparableNetworkUri(place.networkUri)
                : bookmark.path == place.path)
            return index;
    }
    return -1;
}

void PlacesModel::saveBookmarks() const
{
    QStringList paths;
    QStringList labels;
    for (const PlaceEntry &bookmark : m_bookmarks) {
        paths << (bookmark.networkUri.isEmpty() ? bookmark.path : bookmark.networkUri);
        labels << bookmark.label;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("bookmarks/paths"), paths);
    settings.setValue(QStringLiteral("bookmarks/labels"), labels);
}

QVector<PlaceEntry> PlacesModel::collectPlaces(const QVector<PlaceEntry> &bookmarks,
                                               const QStringList &recentNetworkUris,
                                               const QVector<PlaceEntry> &knownDiscoveries,
                                               bool discoverNetworks)
{
    QVector<PlaceEntry> places;
    QSet<QString> paths;
    QSet<QString> networkUris;
    const QVector<PlaceEntry> mountedNetworks = networkMounts();
    auto addLocal = [&places, &paths](const QString &label, const QString &path,
                                     const QString &kind) {
        const QString cleanPath = QDir::cleanPath(path);
        if (cleanPath.isEmpty() || paths.contains(cleanPath) || !QFileInfo(cleanPath).isDir())
            return;
        paths.insert(cleanPath);
        places.append({label, cleanPath, kind, {}, {}, false, false, {}});
    };

    addLocal(QStringLiteral("Home"), QDir::homePath(), QStringLiteral("home"));
    places.append({QStringLiteral("Recent"), {}, QStringLiteral("recent"), {}, {},
                   false, false, {}});
    const struct {
        QStandardPaths::StandardLocation location;
        const char *label;
    } standardPlaces[] = {
        {QStandardPaths::DesktopLocation, "Desktop"},
        {QStandardPaths::DocumentsLocation, "Documents"},
        {QStandardPaths::DownloadLocation, "Downloads"},
        {QStandardPaths::PicturesLocation, "Pictures"},
        {QStandardPaths::MusicLocation, "Music"},
        {QStandardPaths::MoviesLocation, "Videos"},
        {QStandardPaths::TemplatesLocation, "Templates"},
    };
    for (const auto &place : standardPlaces)
        addLocal(QString::fromLatin1(place.label),
                 QStandardPaths::writableLocation(place.location), QStringLiteral("folder"));

    places.append({QStringLiteral("Trash"), {}, QStringLiteral("trash"), {}, {},
                   false, false, {}});
    for (const PlaceEntry &bookmark : bookmarks) {
        if (!bookmark.networkUri.isEmpty()) {
            PlaceEntry resolved = bookmark;
            const QString comparable = comparableNetworkUri(bookmark.networkUri);
            for (const PlaceEntry &mounted : mountedNetworks) {
                if (comparableNetworkUri(mounted.networkUri) == comparable) {
                    resolved.path = mounted.path;
                    resolved.mounted = true;
                    break;
                }
            }
            places.append(resolved);
            networkUris.insert(comparable);
            if (!resolved.path.isEmpty())
                paths.insert(resolved.path);
        } else if (QFileInfo(bookmark.path).isDir() && !paths.contains(bookmark.path)) {
            places.append(bookmark);
            paths.insert(bookmark.path);
        }
    }
    addLocal(QStringLiteral("File System"), QStringLiteral("/"), QStringLiteral("volume"));

    QVector<PlaceEntry> volumes;
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady() || storage.rootPath() == QStringLiteral("/"))
            continue;
        const QString path = QDir::cleanPath(storage.rootPath());
        if (paths.contains(path) || path.startsWith(QStringLiteral("/boot"))
            || path.startsWith(QStringLiteral("/var/"))
            || path.contains(QStringLiteral("/gvfs/")))
            continue;
        const QByteArray device = storage.device();
        const bool userMount = path.startsWith(QStringLiteral("/run/media/"))
            || path.startsWith(QStringLiteral("/media/"))
            || path.startsWith(QStringLiteral("/mnt/"));
        if (!userMount && !device.startsWith("/dev/"))
            continue;
        QString label = storage.displayName();
        if (label.isEmpty())
            label = storage.name();
        if (label.isEmpty())
            label = QFileInfo(path).fileName();
        volumes.append({label.isEmpty() ? path : label, path, QStringLiteral("volume"),
                        {}, {}, true, false, {}});
        paths.insert(path);
    }
    std::sort(volumes.begin(), volumes.end(), [](const PlaceEntry &left, const PlaceEntry &right) {
        return QString::localeAwareCompare(left.label, right.label) < 0;
    });
    places += volumes;

    const QString lsblk = QStandardPaths::findExecutable(QStringLiteral("lsblk"));
    if (!lsblk.isEmpty()) {
        QProcess process;
        process.start(lsblk,
                      {QStringLiteral("--json"), QStringLiteral("--paths"),
                       QStringLiteral("--output"),
                       QStringLiteral("NAME,PATH,LABEL,MOUNTPOINTS,RM,HOTPLUG,TYPE,FSTYPE")},
                      QIODevice::ReadOnly);
        if (process.waitForFinished(3000) && process.exitCode() == 0) {
            const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput());
            QVector<PlaceEntry> devices;
            collectBlockDevices(document.object().value(QStringLiteral("blockdevices")).toArray(),
                                false, {}, devices);
            for (const PlaceEntry &device : std::as_const(devices)) {
                if (device.mounted && paths.contains(device.path)) {
                    for (PlaceEntry &place : places) {
                        if (place.path == device.path) {
                            place.devicePath = device.devicePath;
                            place.ejectPath = device.ejectPath;
                            place.mounted = true;
                            place.ejectable = device.ejectable;
                            break;
                        }
                    }
                } else {
                    places.append(device);
                    if (device.mounted)
                        paths.insert(device.path);
                }
            }
        }
    }
    for (const PlaceEntry &network : mountedNetworks) {
        const QString comparable = comparableNetworkUri(network.networkUri);
        if (networkUris.contains(comparable))
            continue;
        places.append(network);
        networkUris.insert(comparable);
        paths.insert(network.path);
    }
    for (const QString &recent : recentNetworkUris) {
        const QString comparable = comparableNetworkUri(recent);
        if (comparable.isEmpty() || networkUris.contains(comparable))
            continue;
        places.append({networkLabel(comparable), {}, QStringLiteral("network-recent"),
                       {}, {}, false, false, comparable});
        networkUris.insert(comparable);
    }
    const QVector<PlaceEntry> discoveries = discoverNetworks
        ? discoveredNetworkPlaces() : knownDiscoveries;
    for (const PlaceEntry &discovery : discoveries) {
        const QString comparable = comparableNetworkUri(discovery.networkUri);
        if (comparable.isEmpty() || networkUris.contains(comparable))
            continue;
        places.append(discovery);
        networkUris.insert(comparable);
    }
    return places;
}
