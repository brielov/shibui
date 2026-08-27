#include "filesystemmodel.h"

#include <QDesktopServices>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QMimeData>
#include <QProcess>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QtConcurrentRun>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <unistd.h>

namespace {
constexpr QDir::Filters baseFilters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;

int sourceColumnForSortField(int field)
{
    switch (field) {
    case FileSystemModel::SizeSort:
        return 1;
    case FileSystemModel::TypeSort:
        return 2;
    case FileSystemModel::ModifiedSort:
        return 3;
    case FileSystemModel::NameSort:
    default:
        return 0;
    }
}

QString desktopFilePath(const QString &desktopId)
{
    if (desktopId.isEmpty())
        return {};
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  QStringLiteral("applications/") + desktopId);
}

QStringList expandedDesktopCommand(const QString &desktopPath, const QString &filePath)
{
    QSettings desktop(desktopPath, QSettings::IniFormat);
    desktop.beginGroup(QStringLiteral("Desktop Entry"));
    const QString exec = desktop.value(QStringLiteral("Exec")).toString();
    const QString name = desktop.value(QStringLiteral("Name")).toString();
    const QString icon = desktop.value(QStringLiteral("Icon")).toString();
    desktop.endGroup();

    QStringList command;
    const QString url = QUrl::fromLocalFile(filePath).toString();
    const QStringList tokens = QProcess::splitCommand(exec);
    for (QString token : tokens) {
        if (token == QStringLiteral("%f") || token == QStringLiteral("%F")) {
            command << filePath;
        } else if (token == QStringLiteral("%u") || token == QStringLiteral("%U")) {
            command << url;
        } else if (token == QStringLiteral("%i")) {
            if (!icon.isEmpty())
                command << QStringLiteral("--icon") << icon;
        } else if (token == QStringLiteral("%c")) {
            command << name;
        } else if (token == QStringLiteral("%k")) {
            command << desktopPath;
        } else {
            token.replace(QStringLiteral("%%"), QStringLiteral("%"));
            token.replace(QStringLiteral("%f"), filePath);
            token.replace(QStringLiteral("%F"), filePath);
            token.replace(QStringLiteral("%u"), url);
            token.replace(QStringLiteral("%U"), url);
            token.remove(QStringLiteral("%i"));
            token.remove(QStringLiteral("%c"));
            token.remove(QStringLiteral("%k"));
            if (!token.isEmpty())
                command << token;
        }
    }
    return command;
}

QHash<QString, QStringList> readTrashMetadata(const QString &infoPath)
{
    QHash<QString, QStringList> metadata;
    QDirIterator iterator(infoPath, {QStringLiteral("*.trashinfo")}, QDir::Files);
    while (iterator.hasNext()) {
        const QString entryPath = iterator.next();
        QFile entry(entryPath);
        if (!entry.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QString originalPath;
        QString deletionDate;
        while (!entry.atEnd()) {
            const QByteArray line = entry.readLine().trimmed();
            if (line.startsWith("Path="))
                originalPath = QUrl::fromPercentEncoding(line.mid(5));
            else if (line.startsWith("DeletionDate="))
                deletionDate = QString::fromUtf8(line.mid(13));
        }

        QString trashName = QFileInfo(entryPath).fileName();
        trashName.chop(QStringLiteral(".trashinfo").size());
        const QString trashPath = QDir(QFileInfo(infoPath).absolutePath())
                                      .filePath(QStringLiteral("files/") + trashName);
        metadata.insert(QDir::cleanPath(trashPath), {originalPath, deletionDate, entryPath});
    }
    return metadata;
}

struct TrashLocation
{
    QString filesPath;
    QString infoPath;
};

QVector<TrashLocation> availableTrashLocations()
{
    QVector<TrashLocation> locations;
    QSet<QString> seen;
    auto addLocation = [&](const QString &root) {
        const QString filesPath = QDir(root).filePath(QStringLiteral("files"));
        const QString infoPath = QDir(root).filePath(QStringLiteral("info"));
        if (!QFileInfo(filesPath).isDir() || !QFileInfo(infoPath).isDir())
            return;
        const QString key = QDir::cleanPath(filesPath);
        if (!seen.contains(key)) {
            seen.insert(key);
            locations.append({key, QDir::cleanPath(infoPath)});
        }
    };

    addLocation(QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                    .filePath(QStringLiteral("Trash")));
    for (const QString &root : qEnvironmentVariable("SHIBUI_ADDITIONAL_TRASH_ROOTS")
                                   .split(QLatin1Char(':'), Qt::SkipEmptyParts))
        addLocation(root);
    const QString uid = QString::number(getuid());
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady() || storage.isReadOnly())
            continue;
        const QString root = storage.rootPath();
        addLocation(QDir(root).filePath(QStringLiteral(".Trash/%1").arg(uid)));
        addLocation(QDir(root).filePath(QStringLiteral(".Trash-%1").arg(uid)));
    }
    return locations;
}
}

struct FileSystemModel::TransferConflictControl
{
    std::mutex mutex;
    std::condition_variable changed;
    bool decisionReady = false;
    TransferConflictDecision decision;
};

FileSystemModel::FileSystemModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_source.setReadOnly(true);
    m_source.setOptions(QFileSystemModel::DontUseCustomDirectoryIcons
                        | QFileSystemModel::DontResolveSymlinks);
    updateSourceFilter();

    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(50);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &FileSystemModel::rebuildRows);

    connect(&m_source, &QFileSystemModel::directoryLoaded, this, [this](const QString &path) {
        if (normalizedPath(path) != m_currentPath)
            return;
        setLoading(false);
        scheduleRebuild();
    });

    connect(&m_source, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex &parent, int, int) {
                if (!isRootParent(parent))
                    return;
                setLoading(false);
                scheduleRebuild();
            });
    connect(&m_source, &QAbstractItemModel::rowsRemoved, this,
            [this](const QModelIndex &parent, int, int) {
                if (isRootParent(parent))
                    scheduleRebuild();
            });
    connect(&m_source, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
                if (!isRootParent(topLeft.parent()) && !isRootParent(bottomRight.parent()))
                    return;
                for (int sourceRow = topLeft.row(); sourceRow <= bottomRight.row(); ++sourceRow) {
                    const QModelIndex sourceIndex = m_source.index(sourceRow, 0, m_rootIndex);
                    const QString path = normalizedPath(m_source.filePath(sourceIndex));
                    const auto mapped = m_rowByPath.constFind(path);
                    if (mapped != m_rowByPath.cend())
                        emit dataChanged(index(mapped.value()), index(mapped.value()));
                }
            });
    connect(&m_source, &QAbstractItemModel::layoutChanged, this,
            &FileSystemModel::scheduleRebuild);
    connect(&m_source, &QAbstractItemModel::modelReset, this,
            &FileSystemModel::scheduleRebuild);
    connect(&m_pathWatcher, &QFileSystemWatcher::directoryChanged, this,
            &FileSystemModel::handlePathWatchChange);
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged,
            this, &FileSystemModel::fileClipboardChanged);

    connect(&m_transferWatcher, &QFutureWatcher<TransferResult>::finished, this, [this] {
        const TransferResult result = m_transferWatcher.result();
        const bool wasUndo = m_undoActive;
        const bool wasMove = m_transferMove;
        const bool wasRestore = m_transferRestore;
        const QString createdDirectory = m_transferCreatedDirectory;
        m_transferCancelled.reset();
        m_transferConflictControl.reset();
        m_transferProgress = 0;
        m_transferPhase.clear();
        m_transferCurrentPath.clear();
        m_transferDestination.clear();
        m_transferActive = false;
        m_transferRestore = false;
        m_transferCreatedDirectory.clear();
        m_undoActive = false;
        clearTransferConflict();
        if (wasUndo) {
            finishUndo(result);
        } else if (wasMove && !wasRestore && !result.completedSources.isEmpty()) {
            pushUndo({createdDirectory.isEmpty() ? UndoKind::Move
                                                 : UndoKind::MoveIntoNewFolder,
                      result.completedSources,
                      result.destinationPaths,
                      result.replacedTargetPaths,
                      result.replacedTrashPaths,
                      result.replacedTrashInfoPaths,
                      createdDirectory});
        } else if (!createdDirectory.isEmpty()) {
            const QFileInfo directory(createdDirectory);
            if (directory.isDir()
                && QDir(createdDirectory)
                       .entryList(QDir::AllEntries | QDir::Hidden | QDir::System
                                  | QDir::NoDotAndDotDot).isEmpty())
                QDir(directory.absolutePath()).rmdir(directory.fileName());
        }
        if (!result.success && !result.cancelled)
            setErrorMessage(result.message);
        emit transferChanged();
        if (!wasUndo) {
            emit transferFinished(result.success, result.cancelled, result.message,
                                  result.completedSources, result.destinationPaths);
        }
        scheduleRebuild();
    });

    connect(&m_trashWatcher, &QFutureWatcher<TrashResult>::finished, this, [this] {
        const TrashResult result = m_trashWatcher.result();
        const bool wasEmptyTrash = m_emptyTrashOperation;
        m_trashCancelled.reset();
        m_trashProgress = 0;
        m_trashCurrentPath.clear();
        m_trashActive = false;
        m_emptyTrashOperation = false;
        if (wasEmptyTrash) {
            removeTrashUndoActions();
        } else if (!result.trashedSources.isEmpty()) {
            QStringList infoPaths;
            infoPaths.reserve(result.trashPaths.size());
            for (const QString &trashPath : result.trashPaths) {
                const QString trashRoot = QFileInfo(QFileInfo(trashPath).absolutePath())
                                              .absolutePath();
                infoPaths << QDir(trashRoot).filePath(
                    QStringLiteral("info/%1.trashinfo")
                        .arg(QFileInfo(trashPath).fileName()));
            }
            pushUndo({UndoKind::Trash, result.trashedSources, result.trashPaths,
                      {}, {}, infoPaths, {}});
        }
        if (!result.success && (!result.cancelled || !result.failedPaths.isEmpty()))
            setErrorMessage(result.message);
        emit trashChanged();
        emit trashFinished(result.success, result.cancelled, result.message,
                           result.trashedSources, result.trashPaths, result.failedPaths);
        scheduleRebuild();
    });

    connect(&m_trashMetadataWatcher,
            &QFutureWatcher<QHash<QString, QStringList>>::finished,
            this, [this] {
                if (!m_trashView)
                    return;
                m_trashMetadata = m_trashMetadataWatcher.result();
                scheduleRebuild();
                if (m_trashMetadataReloadPending)
                    loadTrashMetadata();
            });
}

FileSystemModel::~FileSystemModel()
{
    if (m_transferCancelled)
        m_transferCancelled->store(true);
    if (m_transferConflictControl)
        m_transferConflictControl->changed.notify_all();
    if (m_trashCancelled)
        m_trashCancelled->store(true);
    m_transferWatcher.waitForFinished();
    m_trashWatcher.waitForFinished();
    m_trashMetadataWatcher.waitForFinished();
}

int FileSystemModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : (m_trashView ? m_trashRows.size() : m_rows.size());
}

QVariant FileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const QFileInfo info = fileInfoAt(index.row());
    if (!info.exists() && !info.isSymLink())
        return {};
    const QStringList trashInfo = m_trashView
        ? m_trashMetadata.value(QDir::cleanPath(info.absoluteFilePath())) : QStringList();
    const QString originalPath = trashInfo.value(0);
    const QDateTime deletionDate = QDateTime::fromString(trashInfo.value(1), Qt::ISODate);

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return originalPath.isEmpty() ? info.fileName() : QFileInfo(originalPath).fileName();
    case PathRole:
        return info.absoluteFilePath();
    case DirectoryRole:
        return info.isDir();
    case SymlinkRole:
        return info.isSymLink();
    case BrokenSymlinkRole:
        return info.isSymLink() && !QFileInfo::exists(info.symLinkTarget());
    case SizeRole:
        return info.isDir() ? QVariant::fromValue<qint64>(-1) : info.size();
    case SizeTextRole:
        return info.isDir() ? QStringLiteral("—")
                            : QLocale().formattedDataSize(info.size(), 1, QLocale::DataSizeTraditionalFormat);
    case TypeTextRole:
        return m_trashView
            ? (originalPath.isEmpty() ? tr("Unknown location") : QFileInfo(originalPath).absolutePath())
            : typeText(info);
    case ModifiedRole:
        return m_trashView && deletionDate.isValid() ? deletionDate : info.lastModified();
    case ModifiedTextRole:
        return m_trashView
            ? (deletionDate.isValid() ? QLocale().toString(deletionDate, QLocale::ShortFormat)
                                      : QStringLiteral("—"))
            : QLocale().toString(info.lastModified(), QLocale::ShortFormat);
    case IconSourceRole:
        return iconSource(info.absoluteFilePath());
    case ThumbnailSourceRole: {
        static const QStringList imageExtensions = {
            QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
            QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp"),
            QStringLiteral("heic"), QStringLiteral("avif"),
        };
        const bool remote = info.absoluteFilePath().contains(QStringLiteral("/gvfs/"));
        return !remote && !info.isDir()
            && imageExtensions.contains(info.suffix().toCaseFolded())
            ? QUrl::fromLocalFile(info.absoluteFilePath()).toString() : QString();
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> FileSystemModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "filePath"},
        {DirectoryRole, "isDirectory"},
        {SymlinkRole, "isSymlink"},
        {BrokenSymlinkRole, "isBrokenSymlink"},
        {SizeRole, "fileSize"},
        {SizeTextRole, "sizeText"},
        {TypeTextRole, "typeText"},
        {ModifiedRole, "modified"},
        {ModifiedTextRole, "modifiedText"},
        {IconSourceRole, "iconSource"},
        {ThumbnailSourceRole, "thumbnailSource"},
    };
}

QString FileSystemModel::currentPath() const
{
    return m_currentPath;
}

QString FileSystemModel::filterText() const
{
    return m_filterText;
}

void FileSystemModel::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;
    m_filterText = text;
    emit filterTextChanged();
    scheduleRebuild();
}

bool FileSystemModel::showHidden() const
{
    return m_showHidden;
}

void FileSystemModel::setShowHidden(bool show)
{
    if (m_showHidden == show)
        return;
    m_showHidden = show;
    emit showHiddenChanged();
    setLoading(true);
    updateSourceFilter();
    scheduleRebuild();
}

bool FileSystemModel::loading() const
{
    return m_loading;
}

QString FileSystemModel::errorMessage() const
{
    return m_errorMessage;
}

int FileSystemModel::sortField() const
{
    return m_sortField;
}

bool FileSystemModel::sortAscending() const
{
    return m_sortOrder == Qt::AscendingOrder;
}

QString FileSystemModel::homePath() const
{
    return QDir::homePath();
}

int FileSystemModel::iconRevision() const
{
    return m_iconRevision;
}

void FileSystemModel::setIconRevision(int revision)
{
    if (m_iconRevision == revision)
        return;
    m_iconRevision = revision;
    emit iconRevisionChanged();
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(m_rows.size() - 1), {IconSourceRole});
}

bool FileSystemModel::transferActive() const
{
    return m_transferActive;
}

qreal FileSystemModel::transferProgress() const
{
    return m_transferProgress;
}

QString FileSystemModel::transferPhase() const
{
    return m_transferPhase;
}

QString FileSystemModel::transferCurrentPath() const
{
    return m_transferCurrentPath;
}

QString FileSystemModel::transferDestination() const
{
    return m_transferDestination;
}

bool FileSystemModel::transferMove() const
{
    return m_transferMove;
}

bool FileSystemModel::transferConflictActive() const
{
    return m_transferConflictActive;
}

QString FileSystemModel::transferConflictSource() const
{
    return m_transferConflictSource;
}

QString FileSystemModel::transferConflictTarget() const
{
    return m_transferConflictTarget;
}

bool FileSystemModel::transferConflictSourceIsDirectory() const
{
    return m_transferConflictSourceIsDirectory;
}

bool FileSystemModel::transferConflictTargetIsDirectory() const
{
    return m_transferConflictTargetIsDirectory;
}

QString FileSystemModel::transferConflictError() const
{
    return m_transferConflictError;
}

bool FileSystemModel::canUndo() const
{
    return !m_undoHistory.isEmpty();
}

QString FileSystemModel::undoDescription() const
{
    if (m_undoHistory.isEmpty())
        return {};
    const UndoAction &action = m_undoHistory.constLast();
    const int count = action.afterPaths.size();
    switch (action.kind) {
    case UndoKind::CreateDirectory:
        return tr("create folder “%1”").arg(QFileInfo(action.afterPaths.constFirst()).fileName());
    case UndoKind::Rename:
        return tr("rename “%1”").arg(QFileInfo(action.afterPaths.constFirst()).fileName());
    case UndoKind::Move:
        return tr("move %1 item%2").arg(count).arg(count == 1 ? QString() : QStringLiteral("s"));
    case UndoKind::MoveIntoNewFolder:
        return tr("create folder “%1” with %2 item%3")
            .arg(QFileInfo(action.createdContainerPath).fileName())
            .arg(count).arg(count == 1 ? QString() : QStringLiteral("s"));
    case UndoKind::Trash:
        return tr("trash %1 item%2").arg(count).arg(count == 1 ? QString() : QStringLiteral("s"));
    }
    return {};
}

bool FileSystemModel::undoActive() const
{
    return m_undoActive;
}

QStringList FileSystemModel::fileClipboardPaths() const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    if (!mimeData)
        return {};

    QList<QUrl> urls = mimeData->urls();
    if (urls.isEmpty() && mimeData->hasFormat(QStringLiteral("x-special/gnome-copied-files"))) {
        const QList<QByteArray> lines = mimeData->data(
            QStringLiteral("x-special/gnome-copied-files")).split('\n');
        for (int index = 1; index < lines.size(); ++index) {
            const QUrl url = QUrl::fromEncoded(lines.at(index).trimmed());
            if (url.isValid())
                urls << url;
        }
    }

    QStringList paths;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        const QString path = QDir::cleanPath(url.toLocalFile());
        if (!path.isEmpty() && !paths.contains(path))
            paths << path;
    }
    return paths;
}

bool FileSystemModel::fileClipboardMove() const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    if (!mimeData)
        return false;
    if (mimeData->hasFormat(QStringLiteral("x-special/gnome-copied-files"))) {
        return mimeData->data(QStringLiteral("x-special/gnome-copied-files"))
            .split('\n').value(0).trimmed() == QByteArrayLiteral("cut");
    }
    return mimeData->data(QStringLiteral("application/x-kde-cutselection")).trimmed()
        == QByteArrayLiteral("1");
}

void FileSystemModel::setFileClipboard(const QStringList &paths, bool move)
{
    if (paths.isEmpty()) {
        QGuiApplication::clipboard()->clear();
        return;
    }

    QList<QUrl> urls;
    QByteArray gnomeData = move ? QByteArrayLiteral("cut") : QByteArrayLiteral("copy");
    for (const QString &rawPath : paths) {
        const QString path = QDir::cleanPath(rawPath);
        if (path.isEmpty())
            continue;
        const QUrl url = QUrl::fromLocalFile(path);
        urls << url;
        gnomeData += '\n' + url.toEncoded();
    }
    if (urls.isEmpty())
        return;

    auto *mimeData = new QMimeData;
    mimeData->setUrls(urls);
    mimeData->setData(QStringLiteral("x-special/gnome-copied-files"), gnomeData);
    mimeData->setData(QStringLiteral("application/x-kde-cutselection"),
                      move ? QByteArrayLiteral("1") : QByteArrayLiteral("0"));
    QGuiApplication::clipboard()->setMimeData(mimeData);
}

void FileSystemModel::clearFileClipboardIfOwned()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard->ownsClipboard())
        clipboard->clear();
}

bool FileSystemModel::ownsFileClipboard() const
{
    return QGuiApplication::clipboard()->ownsClipboard();
}

bool FileSystemModel::copyPathsAsText(const QStringList &paths)
{
    QStringList cleanPaths;
    for (const QString &path : paths) {
        const QString absolute = QFileInfo(path).absoluteFilePath();
        if (!absolute.isEmpty() && !cleanPaths.contains(absolute))
            cleanPaths << absolute;
    }
    if (cleanPaths.isEmpty())
        return false;
    QGuiApplication::clipboard()->setText(cleanPaths.join(QLatin1Char('\n')));
    return true;
}

bool FileSystemModel::openTerminal(const QString &path)
{
    QFileInfo info(path);
    const QString directory = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    if (!QFileInfo(directory).isDir()) {
        setErrorMessage(QStringLiteral("The terminal location is unavailable."));
        return false;
    }
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("xdg-terminal-exec"));
    if (executable.isEmpty()
        || !QProcess::startDetached(executable,
                                    {QStringLiteral("--dir=%1").arg(directory)})) {
        setErrorMessage(QStringLiteral("Could not start the configured terminal."));
        return false;
    }
    return true;
}

bool FileSystemModel::trashView() const
{
    return m_trashView;
}

bool FileSystemModel::trashActive() const
{
    return m_trashActive;
}

qreal FileSystemModel::trashProgress() const
{
    return m_trashProgress;
}

QString FileSystemModel::trashCurrentPath() const
{
    return m_trashCurrentPath;
}

bool FileSystemModel::navigateTo(const QString &path)
{
    const QString normalized = normalizedPath(path, m_currentPath);
    const QFileInfo info(normalized);

    if (normalized.isEmpty() || !info.exists()) {
        setErrorMessage(tr("Location does not exist: %1").arg(path));
        return false;
    }
    if (!info.isDir()) {
        setErrorMessage(tr("Not a directory: %1").arg(normalized));
        return false;
    }
    if (!info.isReadable()) {
        setErrorMessage(tr("Permission denied: %1").arg(normalized));
        return false;
    }

    const bool leavingTrash = m_trashView;
    m_trashView = false;
    m_trashMetadata.clear();
    m_trashRows.clear();
    m_trashFilesPaths.clear();
    m_trashInfoPaths.clear();
    setErrorMessage({});
    setLoading(true);

    const bool pathChanged = normalized != m_currentPath;
    m_currentPath = normalized;
    m_rootIndex = m_source.setRootPath(m_currentPath);
    m_source.sort(sourceColumnForSortField(m_sortField), m_sortOrder);
    updatePathWatches();

    beginResetModel();
    m_rows.clear();
    m_rowByPath.clear();
    endResetModel();
    emit countChanged();
    emit contentsChanged();

    if (pathChanged)
        emit currentPathChanged();
    if (leavingTrash)
        emit trashViewChanged();
    m_rebuildTimer.start(0);
    return true;
}

bool FileSystemModel::navigateToTrash()
{
    if (m_trashView)
        return true;
    if (transferActive() || trashActive()) {
        setErrorMessage(tr("Wait for the active file operation to finish."));
        return false;
    }

    const QString trashRoot = QDir(QStandardPaths::writableLocation(
                                        QStandardPaths::GenericDataLocation))
                                  .filePath(QStringLiteral("Trash"));
    const QString filesPath = QDir(trashRoot).filePath(QStringLiteral("files"));
    const QString infoPath = QDir(trashRoot).filePath(QStringLiteral("info"));
    if (!QDir().mkpath(filesPath) || !QDir().mkpath(infoPath)) {
        setErrorMessage(tr("Could not open the Trash location."));
        return false;
    }

    m_pathBeforeTrash = m_currentPath;
    m_trashView = true;
    m_trashMetadata.clear();
    m_trashRows.clear();
    m_trashFilesPaths.clear();
    m_trashInfoPaths.clear();
    for (const TrashLocation &location : availableTrashLocations()) {
        m_trashFilesPaths << location.filesPath;
        m_trashInfoPaths << location.infoPath;
    }
    setErrorMessage({});
    setLoading(true);

    m_currentPath = QDir::cleanPath(filesPath);
    m_rootIndex = m_source.setRootPath(m_currentPath);
    m_source.sort(sourceColumnForSortField(m_sortField), m_sortOrder);
    updatePathWatches();

    beginResetModel();
    m_rows.clear();
    m_trashRows.clear();
    m_rowByPath.clear();
    endResetModel();
    emit countChanged();
    emit contentsChanged();
    emit currentPathChanged();
    emit trashViewChanged();
    loadTrashMetadata();
    m_rebuildTimer.start(0);
    return true;
}

bool FileSystemModel::goParent()
{
    if (m_trashView)
        return navigateTo(m_pathBeforeTrash.isEmpty() ? QDir::homePath() : m_pathBeforeTrash);
    if (m_currentPath.isEmpty())
        return false;
    QDir directory(m_currentPath);
    if (!directory.cdUp())
        return false;
    return navigateTo(directory.absolutePath());
}

bool FileSystemModel::activate(int row)
{
    const QFileInfo info = fileInfoAt(row);
    if (!info.exists() && !info.isSymLink())
        return false;

    if (m_trashView && info.isDir()) {
        setErrorMessage(tr("Restore this folder before opening it."));
        return false;
    }

    if (info.isDir())
        return navigateTo(info.absoluteFilePath());

    if (info.isSymLink() && !QFileInfo::exists(info.symLinkTarget())) {
        setErrorMessage(tr("Broken symbolic link: %1").arg(info.fileName()));
        return false;
    }

    launchFile(info);
    return true;
}

bool FileSystemModel::activatePath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) {
        setErrorMessage(QStringLiteral("The selected search result no longer exists."));
        return false;
    }
    if (info.isDir())
        return navigateTo(info.absoluteFilePath());
    launchFile(info);
    return true;
}

bool FileSystemModel::openWith(const QString &path, const QString &desktopId)
{
    const QFileInfo info(path);
    const QString desktopPath = desktopFilePath(desktopId);
    const QString gio = QStandardPaths::findExecutable(QStringLiteral("gio"));
    if (!info.isFile() || desktopPath.isEmpty() || gio.isEmpty()) {
        setErrorMessage(QStringLiteral("The selected application cannot open this file."));
        return false;
    }
    if (!QProcess::startDetached(gio,
                                 {QStringLiteral("launch"), desktopPath,
                                  info.absoluteFilePath()})) {
        setErrorMessage(QStringLiteral("Could not start the selected application."));
        return false;
    }
    return true;
}

QString FileSystemModel::pathAt(int row) const
{
    return fileInfoAt(row).absoluteFilePath();
}

bool FileSystemModel::isDirectoryAt(int row) const
{
    return fileInfoAt(row).isDir();
}

int FileSystemModel::indexOfPath(const QString &path) const
{
    const QString wanted = QDir::cleanPath(QDir::fromNativeSeparators(path));
    return m_rowByPath.value(wanted, -1);
}

QString FileSystemModel::createDirectory(const QString &name)
{
    if (m_trashView) {
        setErrorMessage(tr("Folders cannot be created inside Trash."));
        return {};
    }
    if (name.isEmpty()) {
        setErrorMessage(tr("Folder name cannot be empty."));
        return {};
    }
    if (name == QStringLiteral(".") || name == QStringLiteral("..")
        || name.contains(QLatin1Char('/'))) {
        setErrorMessage(tr("Folder name must be a single valid name."));
        return {};
    }
    if (m_currentPath.isEmpty() || !QFileInfo(m_currentPath).isWritable()) {
        setErrorMessage(tr("This directory is not writable."));
        return {};
    }

    QDir directory(m_currentPath);
    const QString targetPath = directory.absoluteFilePath(name);
    const QFileInfo target(targetPath);
    if (target.exists() || target.isSymLink()) {
        setErrorMessage(tr("An item named “%1” already exists.").arg(name));
        return {};
    }
    if (!directory.mkdir(name)) {
        setErrorMessage(tr("Could not create folder “%1”. Check permissions.").arg(name));
        return {};
    }

    setErrorMessage({});
    const QString createdPath = QDir::cleanPath(targetPath);
    pushUndo({UndoKind::CreateDirectory, {}, {createdPath}, {}, {}, {}, {}});
    return createdPath;
}

bool FileSystemModel::startFolderWithSelection(const QString &name,
                                               const QStringList &paths)
{
    if (m_trashView || transferActive() || trashActive()) {
        setErrorMessage(tr("Wait until ordinary file operations are available."));
        return false;
    }
    if (paths.isEmpty()) {
        setErrorMessage(tr("Select at least one item to move into the new folder."));
        return false;
    }
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..")
        || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))) {
        setErrorMessage(tr("Folder name must be a single valid name."));
        return false;
    }
    if (m_currentPath.isEmpty() || !QFileInfo(m_currentPath).isWritable()) {
        setErrorMessage(tr("This directory is not writable."));
        return false;
    }
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if ((!info.exists() && !info.isSymLink())
            || QDir::cleanPath(info.absolutePath()) != m_currentPath) {
            setErrorMessage(tr("The selection changed before the folder could be created."));
            return false;
        }
    }

    QDir directory(m_currentPath);
    const QString targetPath = QDir::cleanPath(directory.absoluteFilePath(name));
    if (QFileInfo::exists(targetPath) || QFileInfo(targetPath).isSymLink()) {
        setErrorMessage(tr("An item named “%1” already exists.").arg(name));
        return false;
    }
    if (!directory.mkdir(name)) {
        setErrorMessage(tr("Could not create folder “%1”. Check permissions.").arg(name));
        return false;
    }
    if (!startTransfer(paths, targetPath, true)) {
        directory.rmdir(name);
        return false;
    }
    m_transferCreatedDirectory = targetPath;
    return true;
}

QString FileSystemModel::renamePath(const QString &path, const QString &newName)
{
    if (m_trashView) {
        setErrorMessage(tr("Restore an item before renaming it."));
        return {};
    }
    if (newName.isEmpty()) {
        setErrorMessage(tr("Name cannot be empty."));
        return {};
    }
    if (newName == QStringLiteral(".") || newName == QStringLiteral("..")
        || newName.contains(QLatin1Char('/'))) {
        setErrorMessage(tr("Name must be a single valid name."));
        return {};
    }

    const QString sourcePath = QDir::cleanPath(
        QDir::isAbsolutePath(path) ? path : QDir(m_currentPath).absoluteFilePath(path));
    const QFileInfo source(sourcePath);
    if (QDir::cleanPath(source.absolutePath()) != QDir::cleanPath(m_currentPath)
        || (!source.exists() && !source.isSymLink())) {
        setErrorMessage(tr("The item to rename no longer exists in this directory."));
        return {};
    }
    if (!QFileInfo(m_currentPath).isWritable()) {
        setErrorMessage(tr("This directory is not writable."));
        return {};
    }
    if (source.fileName() == newName) {
        setErrorMessage({});
        return source.absoluteFilePath();
    }

    QDir directory(m_currentPath);
    const QString targetPath = directory.absoluteFilePath(newName);
    const QFileInfo target(targetPath);
    if (target.exists() || target.isSymLink()) {
        setErrorMessage(tr("An item named “%1” already exists.").arg(newName));
        return {};
    }
    if (!directory.rename(source.fileName(), newName)) {
        setErrorMessage(tr("Could not rename “%1”. Check permissions.").arg(source.fileName()));
        return {};
    }

    setErrorMessage({});
    const QString renamedPath = QDir::cleanPath(targetPath);
    pushUndo({UndoKind::Rename, {sourcePath}, {renamedPath}, {}, {}, {}, {}});
    return renamedPath;
}

void FileSystemModel::clearError()
{
    setErrorMessage({});
}

bool FileSystemModel::startTransfer(const QStringList &sources,
                                    const QString &destinationDirectory,
                                    bool move)
{
    if (m_trashView) {
        setErrorMessage(tr("Restore items instead of pasting into Trash."));
        return false;
    }
    if (transferActive() || trashActive()) {
        setErrorMessage(tr("Another file operation is already active."));
        return false;
    }
    if (sources.isEmpty()) {
        setErrorMessage(tr("There is nothing to paste."));
        return false;
    }

    const QString destinationPath = QDir::cleanPath(
        QDir::isAbsolutePath(destinationDirectory)
            ? destinationDirectory
            : QDir(m_currentPath).absoluteFilePath(destinationDirectory));
    const QFileInfo destination(destinationPath);
    if (!destination.isDir()) {
        setErrorMessage(tr("Paste destination is not a directory."));
        return false;
    }
    if (!destination.isWritable()) {
        setErrorMessage(tr("Paste destination is not writable."));
        return false;
    }

    setErrorMessage({});
    m_transferCancelled = std::make_shared<std::atomic_bool>(false);
    m_transferConflictControl = std::make_shared<TransferConflictControl>();
    m_transferProgress = -1;
    m_transferPhase = tr("Preparing");
    m_transferCurrentPath.clear();
    m_transferDestination = destinationPath;
    m_transferActive = true;
    m_transferMove = move;
    m_transferRestore = false;
    m_transferCreatedDirectory.clear();
    clearTransferConflict();

    const auto cancelled = m_transferCancelled;
    const auto resolveConflict = makeTransferConflictResolver(cancelled);
    const QFuture<TransferResult> future = QtConcurrent::run(
        [this, sources, destinationPath, move, cancelled, resolveConflict] {
            return runFileTransfer(
                sources, destinationPath, move, cancelled,
                [this](const TransferUpdate &update) {
                    QMetaObject::invokeMethod(this, [this, update] {
                        if (transferActive())
                            updateTransferProgress(update);
                    }, Qt::QueuedConnection);
                },
                resolveConflict);
        });
    m_transferWatcher.setFuture(future);
    emit transferChanged();
    return true;
}

void FileSystemModel::cancelTransfer()
{
    if (!transferActive() || !m_transferCancelled)
        return;
    m_transferCancelled->store(true);
    clearTransferConflict();
    m_transferPhase = tr("Cancelling");
    if (m_transferConflictControl)
        m_transferConflictControl->changed.notify_all();
    emit transferChanged();
}

bool FileSystemModel::resolveTransferConflict(const QString &action,
                                              const QString &newName,
                                              bool applyToRemaining)
{
    if (!m_transferActive || !m_transferConflictActive || !m_transferConflictControl)
        return false;

    TransferConflictAction resolvedAction;
    if (action == QStringLiteral("replace"))
        resolvedAction = TransferConflictAction::Replace;
    else if (action == QStringLiteral("skip"))
        resolvedAction = TransferConflictAction::Skip;
    else if (action == QStringLiteral("rename"))
        resolvedAction = TransferConflictAction::Rename;
    else if (action == QStringLiteral("cancel"))
        resolvedAction = TransferConflictAction::Cancel;
    else
        return false;

    QString resolvedName = newName;
    if (resolvedAction == TransferConflictAction::Rename) {
        if (resolvedName.isEmpty() || resolvedName == QStringLiteral(".")
            || resolvedName == QStringLiteral("..") || resolvedName.contains(QLatin1Char('/'))) {
            m_transferConflictError = tr("Enter a single valid name.");
            emit transferChanged();
            return false;
        }
        const QString candidate = QDir(QFileInfo(m_transferConflictTarget).absolutePath())
                                      .absoluteFilePath(resolvedName);
        const QFileInfo candidateInfo(candidate);
        if (candidateInfo.exists() || candidateInfo.isSymLink()) {
            m_transferConflictError = tr("An item named “%1” already exists.").arg(resolvedName);
            emit transferChanged();
            return false;
        }
    }

    {
        const std::lock_guard lock(m_transferConflictControl->mutex);
        m_transferConflictControl->decision = {
            resolvedAction,
            resolvedName,
            applyToRemaining
                && (resolvedAction == TransferConflictAction::Replace
                    || resolvedAction == TransferConflictAction::Skip),
        };
        m_transferConflictControl->decisionReady = true;
    }
    clearTransferConflict();
    m_transferPhase = m_transferRestore ? tr("Restoring")
                                        : (m_transferMove ? tr("Moving") : tr("Copying"));
    emit transferChanged();
    m_transferConflictControl->changed.notify_one();
    return true;
}

bool FileSystemModel::startRestore(const QStringList &paths)
{
    if (!m_trashView) {
        setErrorMessage(tr("Open Trash before restoring items."));
        return false;
    }
    if (transferActive() || trashActive()) {
        setErrorMessage(tr("Another file operation is already active."));
        return false;
    }
    if (paths.isEmpty()) {
        setErrorMessage(tr("There is nothing to restore."));
        return false;
    }

    QVector<RestoreItem> items;
    items.reserve(paths.size());
    for (const QString &path : paths) {
        const QString sourcePath = QDir::cleanPath(path);
        if (!m_trashFilesPaths.contains(QDir::cleanPath(QFileInfo(sourcePath).absolutePath()))) {
            setErrorMessage(tr("Restore items from the current Trash view."));
            return false;
        }
        const QStringList metadata = m_trashMetadata.value(sourcePath);
        if (metadata.size() < 3 || metadata.at(0).isEmpty() || metadata.at(2).isEmpty()) {
            setErrorMessage(tr("Trash metadata is still loading or missing for “%1”.")
                                .arg(QFileInfo(sourcePath).fileName()));
            loadTrashMetadata();
            return false;
        }
        items.append({sourcePath, metadata.at(0), metadata.at(2)});
    }

    setErrorMessage({});
    m_transferCancelled = std::make_shared<std::atomic_bool>(false);
    m_transferConflictControl = std::make_shared<TransferConflictControl>();
    m_transferProgress = -1;
    m_transferPhase = tr("Preparing restore");
    m_transferCurrentPath.clear();
    m_transferDestination = tr("Original locations");
    m_transferActive = true;
    m_transferMove = false;
    m_transferRestore = true;
    clearTransferConflict();

    const auto cancelled = m_transferCancelled;
    const auto resolveConflict = makeTransferConflictResolver(cancelled);
    const QFuture<TransferResult> future = QtConcurrent::run(
        [this, items, cancelled, resolveConflict] {
            return runFileRestore(
                items, cancelled,
                [this](const TransferUpdate &update) {
                    QMetaObject::invokeMethod(this, [this, update] {
                        if (transferActive())
                            updateTransferProgress(update);
                    }, Qt::QueuedConnection);
                },
                resolveConflict);
        });
    m_transferWatcher.setFuture(future);
    emit transferChanged();
    return true;
}

bool FileSystemModel::startEmptyTrash()
{
    if (!m_trashView) {
        setErrorMessage(tr("Open Trash before emptying it."));
        return false;
    }
    if (transferActive() || trashActive()) {
        setErrorMessage(tr("Another file operation is already active."));
        return false;
    }

    setErrorMessage({});
    m_trashCancelled = std::make_shared<std::atomic_bool>(false);
    m_trashProgress = 0;
    m_trashCurrentPath.clear();
    m_trashActive = true;
    m_emptyTrashOperation = true;

    const QStringList filesPaths = m_trashFilesPaths;
    const QStringList infoPaths = m_trashInfoPaths;
    const auto cancelled = m_trashCancelled;
    const QFuture<TrashResult> future = QtConcurrent::run(
        [this, filesPaths, infoPaths, cancelled] {
            TrashResult aggregate;
            aggregate.success = true;
            for (int location = 0; location < filesPaths.size(); ++location) {
                const TrashResult current = runEmptyTrash(
                    filesPaths.at(location), infoPaths.value(location), cancelled,
                    [this](const TrashUpdate &update) {
                    QMetaObject::invokeMethod(this, [this, update] {
                        if (!trashActive())
                            return;
                        m_trashProgress = update.totalItems > 0
                            ? qBound<qreal>(0, qreal(update.completedItems)
                                                 / qreal(update.totalItems), 1)
                            : 1;
                        m_trashCurrentPath = update.currentPath;
                        emit trashChanged();
                    }, Qt::QueuedConnection);
                });
                aggregate.trashedSources += current.trashedSources;
                aggregate.failedPaths += current.failedPaths;
                aggregate.cancelled = current.cancelled;
                aggregate.success = aggregate.success && current.success;
                if (current.cancelled)
                    break;
            }
            const int removed = aggregate.trashedSources.size();
            if (aggregate.cancelled) {
                aggregate.success = false;
                aggregate.message = removed == 0
                    ? QStringLiteral("Empty Trash cancelled.")
                    : QStringLiteral("Removed %1 item%2 before cancelling.")
                          .arg(removed).arg(removed == 1 ? QString() : QStringLiteral("s"));
            } else if (!aggregate.failedPaths.isEmpty()) {
                aggregate.success = false;
                aggregate.message = QStringLiteral("Removed %1 item%2; some items could not be removed.")
                                        .arg(removed)
                                        .arg(removed == 1 ? QString() : QStringLiteral("s"));
            } else {
                aggregate.message = removed == 0
                    ? QStringLiteral("Trash is already empty.")
                    : QStringLiteral("Emptied Trash (%1 item%2).")
                          .arg(removed).arg(removed == 1 ? QString() : QStringLiteral("s"));
            }
            return aggregate;
        });
    m_trashWatcher.setFuture(future);
    emit trashChanged();
    return true;
}

bool FileSystemModel::startTrash(const QStringList &paths)
{
    if (m_trashView) {
        setErrorMessage(tr("Permanent deletion is not available."));
        return false;
    }
    if (transferActive() || trashActive()) {
        setErrorMessage(tr("Another file operation is already active."));
        return false;
    }
    if (paths.isEmpty()) {
        setErrorMessage(tr("There is nothing to move to Trash."));
        return false;
    }
    if (!QFile::supportsMoveToTrash()) {
        setErrorMessage(tr("Trash is not available on this system."));
        return false;
    }

    setErrorMessage({});
    m_trashCancelled = std::make_shared<std::atomic_bool>(false);
    m_trashProgress = 0;
    m_trashCurrentPath.clear();
    m_trashActive = true;
    m_emptyTrashOperation = false;

    const auto cancelled = m_trashCancelled;
    const QFuture<TrashResult> future = QtConcurrent::run(
        [this, paths, cancelled] {
            return runFileTrash(paths, cancelled, [this](const TrashUpdate &update) {
                QMetaObject::invokeMethod(this, [this, update] {
                    if (!trashActive())
                        return;
                    m_trashProgress = update.totalItems > 0
                        ? qBound<qreal>(0, qreal(update.completedItems) / qreal(update.totalItems), 1)
                        : 0;
                    m_trashCurrentPath = update.currentPath;
                    emit trashChanged();
                }, Qt::QueuedConnection);
            });
        });
    m_trashWatcher.setFuture(future);
    emit trashChanged();
    return true;
}

void FileSystemModel::cancelTrash()
{
    if (!trashActive() || !m_trashCancelled)
        return;
    m_trashCancelled->store(true);
    emit trashChanged();
}

bool FileSystemModel::undoLast()
{
    auto fail = [this](const QString &message) {
        setErrorMessage(message);
        emit undoFinished(false, message, {});
        return false;
    };

    if (m_undoHistory.isEmpty())
        return fail(tr("There is nothing to undo."));
    if (transferActive() || trashActive())
        return fail(tr("Wait for the active file operation to finish."));

    const UndoAction action = m_undoHistory.constLast();
    if (action.afterPaths.isEmpty())
        return fail(tr("There is nothing to undo."));

    if (action.kind == UndoKind::CreateDirectory) {
        const QString path = action.afterPaths.constFirst();
        const QFileInfo info(path);
        if (!info.isDir() || info.isSymLink())
            return fail(tr("Cannot undo: the created folder no longer exists at %1.").arg(path));
        if (!QDir(path).entryList(QDir::AllEntries | QDir::Hidden | QDir::System
                                  | QDir::NoDotAndDotDot).isEmpty()) {
            return fail(tr("Cannot undo: the created folder is no longer empty: %1.").arg(path));
        }
        if (!QDir(info.absolutePath()).rmdir(info.fileName()))
            return fail(tr("Could not remove the created folder: %1.").arg(path));

        m_undoHistory.removeLast();
        setErrorMessage({});
        emit undoChanged();
        emit undoFinished(true, tr("Undid folder creation."), {});
        scheduleRebuild();
        return true;
    }

    if (action.kind == UndoKind::Rename) {
        const QString originalPath = action.beforePaths.constFirst();
        const QString renamedPath = action.afterPaths.constFirst();
        const QFileInfo renamed(renamedPath);
        if ((!renamed.exists() && !renamed.isSymLink()) || QFileInfo::exists(originalPath)
            || QFileInfo(originalPath).isSymLink()) {
            return fail(tr("Cannot undo rename because one of the paths changed externally."));
        }
        if (!QDir().rename(renamedPath, originalPath))
            return fail(tr("Could not restore the original name at %1.").arg(originalPath));

        m_undoHistory.removeLast();
        setErrorMessage({});
        emit undoChanged();
        emit undoFinished(true, tr("Undid rename."), {originalPath});
        scheduleRebuild();
        return true;
    }

    m_transferCancelled = std::make_shared<std::atomic_bool>(false);
    m_transferConflictControl.reset();
    m_transferProgress = -1;
    m_transferPhase = tr("Preparing undo");
    m_transferCurrentPath.clear();
    m_transferDestination = tr("Previous locations");
    m_transferActive = true;
    m_transferMove = action.kind == UndoKind::Move
        || action.kind == UndoKind::MoveIntoNewFolder;
    m_transferRestore = false;
    m_transferCreatedDirectory.clear();
    m_undoActive = true;
    clearTransferConflict();
    setErrorMessage({});

    const auto cancelled = m_transferCancelled;
    QFuture<TransferResult> future;
    if (action.kind == UndoKind::Move
        || action.kind == UndoKind::MoveIntoNewFolder) {
        QVector<UndoMoveItem> items;
        items.reserve(action.afterPaths.size());
        for (int index = 0; index < action.afterPaths.size(); ++index) {
            items.append({action.afterPaths.at(index),
                          action.beforePaths.value(index),
                          action.replacedTargetPaths.value(index),
                          action.replacedTrashPaths.value(index),
                          action.replacedTrashInfoPaths.value(index)});
        }
        const QString createdContainerPath = action.createdContainerPath;
        future = QtConcurrent::run([this, items, cancelled, createdContainerPath] {
            return runMoveUndo(items, cancelled, [this](const TransferUpdate &update) {
                QMetaObject::invokeMethod(this, [this, update] {
                    if (transferActive())
                        updateTransferProgress(update);
                }, Qt::QueuedConnection);
            }, createdContainerPath);
        });
    } else {
        QVector<RestoreItem> items;
        items.reserve(action.afterPaths.size());
        for (int index = 0; index < action.afterPaths.size(); ++index) {
            const QString trashPath = action.afterPaths.at(index);
            const QString originalPath = action.beforePaths.value(index);
            if ((!QFileInfo(trashPath).exists() && !QFileInfo(trashPath).isSymLink())
                || QFileInfo::exists(originalPath) || QFileInfo(originalPath).isSymLink()) {
                m_transferCancelled.reset();
                m_transferActive = false;
                m_undoActive = false;
                emit transferChanged();
                emit undoChanged();
                return fail(tr("Cannot undo Trash because one of the paths changed externally."));
            }
            items.append({trashPath, originalPath,
                          action.replacedTrashInfoPaths.value(index)});
        }
        future = QtConcurrent::run([this, items, cancelled] {
            return runFileRestore(
                items, cancelled,
                [this](const TransferUpdate &update) {
                    QMetaObject::invokeMethod(this, [this, update] {
                        if (transferActive())
                            updateTransferProgress(update);
                    }, Qt::QueuedConnection);
                },
                [](const TransferConflict &) {
                    return TransferConflictDecision{TransferConflictAction::Cancel, {}, false};
                });
        });
    }

    m_transferWatcher.setFuture(future);
    emit transferChanged();
    emit undoChanged();
    return true;
}

void FileSystemModel::pushUndo(UndoAction action)
{
    if (action.afterPaths.isEmpty())
        return;
    constexpr int historyLimit = 20;
    while (m_undoHistory.size() >= historyLimit)
        m_undoHistory.removeFirst();
    m_undoHistory.append(std::move(action));
    emit undoChanged();
}

void FileSystemModel::finishUndo(const TransferResult &result)
{
    if (!m_undoHistory.isEmpty()) {
        UndoAction &action = m_undoHistory.last();
        for (int index = action.afterPaths.size() - 1; index >= 0; --index) {
            if (!result.completedSources.contains(action.afterPaths.at(index)))
                continue;
            action.beforePaths.removeAt(index);
            action.afterPaths.removeAt(index);
            if (index < action.replacedTargetPaths.size())
                action.replacedTargetPaths.removeAt(index);
            if (index < action.replacedTrashPaths.size())
                action.replacedTrashPaths.removeAt(index);
            if (index < action.replacedTrashInfoPaths.size())
                action.replacedTrashInfoPaths.removeAt(index);
        }
        if (action.afterPaths.isEmpty())
            m_undoHistory.removeLast();
    }
    emit undoChanged();
    emit undoFinished(result.success, result.message, result.destinationPaths);
}

void FileSystemModel::removeTrashUndoActions()
{
    QVector<UndoAction> remaining;
    remaining.reserve(m_undoHistory.size());
    for (UndoAction &action : m_undoHistory) {
        if (action.kind != UndoKind::Trash)
            remaining.append(std::move(action));
    }
    if (remaining.size() == m_undoHistory.size())
        return;
    m_undoHistory = std::move(remaining);
    emit undoChanged();
}

void FileSystemModel::setSort(int field, bool toggleWhenSame)
{
    if (field < NameSort || field > ModifiedSort)
        return;

    if (m_sortField == field && toggleWhenSame)
        m_sortOrder = m_sortOrder == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
    else if (m_sortField != field) {
        m_sortField = field;
        m_sortOrder = Qt::AscendingOrder;
    }

    m_source.sort(sourceColumnForSortField(m_sortField), m_sortOrder);
    emit sortChanged();
    scheduleRebuild();
}

void FileSystemModel::refresh()
{
    if (m_currentPath.isEmpty())
        return;
    if (!QFileInfo::exists(m_currentPath)) {
        setErrorMessage(tr("Location no longer exists: %1").arg(m_currentPath));
        return;
    }
    setErrorMessage({});
    scheduleRebuild();
}

QString FileSystemModel::normalizedPath(const QString &path, const QString &basePath)
{
    QString expanded = path.trimmed();
    if (expanded == QStringLiteral("~"))
        expanded = QDir::homePath();
    else if (expanded.startsWith(QStringLiteral("~/")))
        expanded = QDir::homePath() + expanded.mid(1);

    if (expanded.startsWith(QStringLiteral("file:"))) {
        const QUrl url(expanded);
        if (url.isLocalFile())
            expanded = url.toLocalFile();
    }

    QDir directory;
    if (QDir::isRelativePath(expanded))
        directory = QDir(basePath.isEmpty() ? QDir::currentPath() : basePath);
    const QString absolute = QDir::isRelativePath(expanded)
        ? directory.absoluteFilePath(expanded)
        : expanded;
    return QDir::cleanPath(QDir::fromNativeSeparators(absolute));
}

bool FileSystemModel::isRootParent(const QModelIndex &parent) const
{
    return parent.isValid() && m_rootIndex.isValid()
        && m_source.filePath(parent) == m_source.filePath(m_rootIndex);
}

QFileInfo FileSystemModel::fileInfoAt(int row) const
{
    if (m_trashView)
        return row >= 0 && row < m_trashRows.size() ? m_trashRows.at(row) : QFileInfo();
    if (row < 0 || row >= m_rows.size() || !m_rows.at(row).isValid())
        return {};
    return m_source.fileInfo(m_rows.at(row));
}

QString FileSystemModel::typeText(const QFileInfo &info) const
{
    if (info.isSymLink() && !QFileInfo::exists(info.symLinkTarget()))
        return tr("Broken link");
    if (info.isDir())
        return tr("Folder");
    const QMimeType mime = m_mimeDatabase.mimeTypeForFile(info, QMimeDatabase::MatchExtension);
    const QString comment = mime.comment();
    return comment.isEmpty() ? mime.name() : comment;
}

QString FileSystemModel::iconSource(const QString &path) const
{
    const QByteArray encoded = path.toUtf8().toBase64(QByteArray::Base64UrlEncoding
                                                       | QByteArray::OmitTrailingEquals);
    return QStringLiteral("image://fileicon/%1?v=%2")
        .arg(QString::fromLatin1(encoded))
        .arg(m_iconRevision);
}

void FileSystemModel::launchFile(const QFileInfo &info)
{
    const QString filePath = info.absoluteFilePath();
    const QString fileName = info.fileName();
    const QString fallbackMimeName = m_mimeDatabase.mimeTypeForFile(info).name();

    auto openWithDesktop = [this, filePath, fileName] {
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath)))
            return;
        const QString message = tr("No application could open %1").arg(fileName);
        setErrorMessage(message);
        emit fileLaunchFailed(message);
    };

    auto queryDefaultApplication = [this, info, filePath, fileName, openWithDesktop](const QString &mimeName) {
        if (mimeName.isEmpty()) {
            openWithDesktop();
            return;
        }

        auto *query = new QProcess(this);
        connect(query, &QProcess::finished, this,
                [this, query, info, filePath, fileName, openWithDesktop](int exitCode,
                                                                        QProcess::ExitStatus status) {
                    const QString desktopId = QString::fromUtf8(query->readAllStandardOutput()).trimmed();
                    query->deleteLater();
                    if (status != QProcess::NormalExit || exitCode != 0 || desktopId.isEmpty()) {
                        openWithDesktop();
                        return;
                    }

                    const QString desktopPath = desktopFilePath(desktopId);
                    if (desktopPath.isEmpty()) {
                        openWithDesktop();
                        return;
                    }
                    QSettings desktop(desktopPath, QSettings::IniFormat);
                    desktop.beginGroup(QStringLiteral("Desktop Entry"));
                    const bool terminal = desktop.value(QStringLiteral("Terminal"), false).toBool();
                    desktop.endGroup();

                    if (!terminal) {
                        openWithDesktop();
                        return;
                    }

                    const QStringList command = expandedDesktopCommand(desktopPath, filePath);
                    if (command.isEmpty()) {
                        openWithDesktop();
                        return;
                    }

                    QStringList terminalArguments = {
                        QStringLiteral("--app-id=shibui-file"),
                        QStringLiteral("--title=%1").arg(fileName),
                        QStringLiteral("--dir=%1").arg(info.absolutePath()),
                        QStringLiteral("--"),
                    };
                    terminalArguments.append(command);
                    if (QProcess::startDetached(QStringLiteral("xdg-terminal-exec"), terminalArguments,
                                                info.absolutePath()))
                        return;

                    const QString message = tr("Could not start the terminal application for %1").arg(fileName);
                    setErrorMessage(message);
                    emit fileLaunchFailed(message);
                });
        connect(query, &QProcess::errorOccurred, this,
                [query, openWithDesktop](QProcess::ProcessError processError) {
                    if (processError != QProcess::FailedToStart)
                        return;
                    query->deleteLater();
                    openWithDesktop();
                });
        query->start(QStringLiteral("xdg-mime"),
                     {QStringLiteral("query"), QStringLiteral("default"), mimeName});
    };

    auto *typeQuery = new QProcess(this);
    connect(typeQuery, &QProcess::finished, this,
            [typeQuery, fallbackMimeName, queryDefaultApplication](int exitCode,
                                                                   QProcess::ExitStatus status) {
                const QString detectedMime = QString::fromUtf8(typeQuery->readAllStandardOutput()).trimmed();
                typeQuery->deleteLater();
                if (status != QProcess::NormalExit || exitCode != 0) {
                    queryDefaultApplication(fallbackMimeName);
                    return;
                }
                queryDefaultApplication(detectedMime.isEmpty() ? fallbackMimeName : detectedMime);
            });
    connect(typeQuery, &QProcess::errorOccurred, this,
            [typeQuery, fallbackMimeName, queryDefaultApplication](QProcess::ProcessError processError) {
                if (processError != QProcess::FailedToStart)
                    return;
                typeQuery->deleteLater();
                queryDefaultApplication(fallbackMimeName);
            });
    typeQuery->start(QStringLiteral("xdg-mime"),
                     {QStringLiteral("query"), QStringLiteral("filetype"), filePath});
}

void FileSystemModel::scheduleRebuild()
{
    m_rebuildTimer.start();
}

void FileSystemModel::rebuildRows()
{
    if (!m_currentPath.isEmpty() && !QFileInfo::exists(m_currentPath)) {
        beginResetModel();
        m_rows.clear();
        m_rowByPath.clear();
        endResetModel();
        setLoading(false);
        setErrorMessage(tr("Location no longer exists: %1").arg(m_currentPath));
        emit countChanged();
        emit contentsChanged();
        return;
    }
    if (m_trashView) {
        QVector<QFileInfo> nextRows;
        const QDir::Filters filters = QDir::AllEntries | QDir::Hidden | QDir::System
            | QDir::NoDotAndDotDot;
        for (const QString &filesPath : std::as_const(m_trashFilesPaths)) {
            const QFileInfoList entries = QDir(filesPath).entryInfoList(filters);
            for (const QFileInfo &info : entries) {
                const QString originalPath = m_trashMetadata
                                                 .value(QDir::cleanPath(info.absoluteFilePath()))
                                                 .value(0);
                const QString displayName = originalPath.isEmpty()
                    ? info.fileName() : QFileInfo(originalPath).fileName();
                if (!m_showHidden && displayName.startsWith(QLatin1Char('.')))
                    continue;
                if (!m_filterText.isEmpty()
                    && !displayName.contains(m_filterText, Qt::CaseInsensitive))
                    continue;
                nextRows.append(info);
            }
        }
        auto less = [this](const QFileInfo &left, const QFileInfo &right) {
            int comparison = 0;
            switch (m_sortField) {
            case SizeSort:
                comparison = left.size() < right.size() ? -1 : left.size() > right.size() ? 1 : 0;
                break;
            case TypeSort:
                comparison = QString::compare(left.suffix(), right.suffix(), Qt::CaseInsensitive);
                break;
            case ModifiedSort:
                comparison = left.lastModified() < right.lastModified() ? -1
                    : left.lastModified() > right.lastModified() ? 1 : 0;
                break;
            case NameSort:
            default:
                comparison = QString::localeAwareCompare(left.fileName(), right.fileName());
                break;
            }
            if (comparison == 0)
                comparison = QString::localeAwareCompare(left.fileName(), right.fileName());
            return m_sortOrder == Qt::AscendingOrder ? comparison < 0 : comparison > 0;
        };
        std::sort(nextRows.begin(), nextRows.end(), less);

        QStringList previousPaths;
        previousPaths.reserve(m_trashRows.size());
        for (const QFileInfo &info : std::as_const(m_trashRows))
            previousPaths << QDir::cleanPath(info.absoluteFilePath());
        QStringList nextPaths;
        nextPaths.reserve(nextRows.size());
        QHash<QString, int> nextRowByPath;
        for (int row = 0; row < nextRows.size(); ++row) {
            const QString path = QDir::cleanPath(nextRows.at(row).absoluteFilePath());
            nextPaths << path;
            nextRowByPath.insert(path, row);
        }
        const bool countChangedValue = nextRows.size() != m_trashRows.size();
        if (previousPaths != nextPaths) {
            beginResetModel();
            m_trashRows = std::move(nextRows);
            m_rowByPath = std::move(nextRowByPath);
            endResetModel();
            if (countChangedValue)
                emit countChanged();
            emit contentsChanged();
        } else {
            m_trashRows = std::move(nextRows);
            m_rowByPath = std::move(nextRowByPath);
            if (!m_trashRows.isEmpty())
                emit dataChanged(index(0), index(m_trashRows.size() - 1));
        }
        setLoading(false);
        return;
    }
    if (!m_rootIndex.isValid())
        return;

    QVector<QPersistentModelIndex> nextRows;
    const int sourceCount = m_source.rowCount(m_rootIndex);
    nextRows.reserve(sourceCount);
    for (int row = 0; row < sourceCount; ++row) {
        const QModelIndex sourceIndex = m_source.index(row, 0, m_rootIndex);
        if (!sourceIndex.isValid())
            continue;
        const QString name = m_source.fileName(sourceIndex);
        const QFileInfo info = m_source.fileInfo(sourceIndex);
        if (!m_showHidden && (name.startsWith(QLatin1Char('.')) || info.isHidden()))
            continue;
        if (!m_filterText.isEmpty() && !name.contains(m_filterText, Qt::CaseInsensitive))
            continue;
        nextRows.append(QPersistentModelIndex(sourceIndex));
    }
    std::stable_partition(nextRows.begin(), nextRows.end(), [this](const QModelIndex &index) {
        return m_source.fileInfo(index).isDir();
    });

    QHash<QString, int> nextRowByPath;
    nextRowByPath.reserve(nextRows.size());
    for (int row = 0; row < nextRows.size(); ++row)
        nextRowByPath.insert(normalizedPath(m_source.filePath(nextRows.at(row))), row);

    const bool sameRows = nextRows == m_rows;
    const bool countChangedValue = nextRows.size() != m_rows.size();
    const QSet<QString> nextPaths(nextRowByPath.keyBegin(), nextRowByPath.keyEnd());
    const QSet<QString> currentPaths(m_rowByPath.keyBegin(), m_rowByPath.keyEnd());
    const bool contentsChangedValue = nextPaths != currentPaths;
    if (sameRows) {
        m_rowByPath = std::move(nextRowByPath);
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(m_rows.size() - 1));
    } else {
        beginResetModel();
        m_rows = std::move(nextRows);
        m_rowByPath = std::move(nextRowByPath);
        endResetModel();
    }
    setLoading(false);
    if (countChangedValue)
        emit countChanged();
    if (contentsChangedValue || !sameRows)
        emit contentsChanged();
}

void FileSystemModel::updateSourceFilter()
{
    QDir::Filters filters = baseFilters;
    if (m_showHidden)
        filters |= QDir::Hidden;
    m_source.setFilter(filters);
}

void FileSystemModel::updatePathWatches()
{
    const QStringList watched = m_pathWatcher.directories();
    if (!watched.isEmpty())
        m_pathWatcher.removePaths(watched);
    if (m_currentPath.isEmpty())
        return;

    QStringList paths = {m_currentPath, QFileInfo(m_currentPath).absolutePath()};
    if (m_trashView) {
        paths += m_trashFilesPaths;
        paths += m_trashInfoPaths;
    }
    paths.removeDuplicates();
    QStringList existing;
    for (const QString &path : paths) {
        if (QFileInfo(path).isDir())
            existing << path;
    }
    if (!existing.isEmpty())
        m_pathWatcher.addPaths(existing);
}

void FileSystemModel::handlePathWatchChange()
{
    if (!QFileInfo::exists(m_currentPath)) {
        scheduleRebuild();
        updatePathWatches();
        return;
    }
    if (m_trashView)
        loadTrashMetadata();
    scheduleRebuild();
    updatePathWatches();
}

void FileSystemModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void FileSystemModel::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message)
        return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

void FileSystemModel::updateTransferProgress(const TransferUpdate &update)
{
    m_transferProgress = update.totalWork > 0
        ? qBound<qreal>(0, qreal(update.completedWork) / qreal(update.totalWork), 1)
        : -1;
    m_transferPhase = update.phase;
    m_transferCurrentPath = update.currentPath;
    emit transferChanged();
}

void FileSystemModel::clearTransferConflict()
{
    m_transferConflictActive = false;
    m_transferConflictSource.clear();
    m_transferConflictTarget.clear();
    m_transferConflictSourceIsDirectory = false;
    m_transferConflictTargetIsDirectory = false;
    m_transferConflictError.clear();
}

TransferConflictResolver FileSystemModel::makeTransferConflictResolver(
    const std::shared_ptr<std::atomic_bool> &cancelled)
{
    const auto conflictControl = m_transferConflictControl;
    return [this, cancelled, conflictControl](const TransferConflict &conflict) {
        {
            const std::lock_guard lock(conflictControl->mutex);
            conflictControl->decisionReady = false;
        }
        QMetaObject::invokeMethod(this, [this, conflict] {
            if (!transferActive())
                return;
            m_transferConflictSource = conflict.sourcePath;
            m_transferConflictTarget = conflict.targetPath;
            m_transferConflictSourceIsDirectory = conflict.sourceIsDirectory;
            m_transferConflictTargetIsDirectory = conflict.targetIsDirectory;
            m_transferConflictError.clear();
            m_transferConflictActive = true;
            m_transferPhase = tr("Waiting for conflict decision");
            emit transferChanged();
        }, Qt::QueuedConnection);

        std::unique_lock lock(conflictControl->mutex);
        conflictControl->changed.wait(lock, [&] {
            return conflictControl->decisionReady || cancelled->load();
        });
        if (cancelled->load())
            return TransferConflictDecision{TransferConflictAction::Cancel, {}, false};
        return conflictControl->decision;
    };
}

void FileSystemModel::loadTrashMetadata()
{
    if (!m_trashView || m_trashInfoPaths.isEmpty())
        return;
    if (m_trashMetadataWatcher.isRunning()) {
        m_trashMetadataReloadPending = true;
        return;
    }
    m_trashMetadataReloadPending = false;
    const QStringList infoPaths = m_trashInfoPaths;
    m_trashMetadataWatcher.setFuture(QtConcurrent::run([infoPaths] {
        QHash<QString, QStringList> metadata;
        for (const QString &infoPath : infoPaths)
            metadata.insert(readTrashMetadata(infoPath));
        return metadata;
    }));
}
