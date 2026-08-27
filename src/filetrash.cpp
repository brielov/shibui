#include "filetrash.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace {
bool removePath(const QString &path,
                const std::shared_ptr<std::atomic_bool> &cancelled = {})
{
    if (cancelled && cancelled->load())
        return false;
    const QFileInfo info(path);
    if (!info.isDir() || info.isSymLink())
        return !info.exists() && !info.isSymLink() ? true : QFile::remove(path);

    QDirIterator iterator(path,
                          QDir::AllEntries | QDir::Hidden | QDir::System
                              | QDir::NoDotAndDotDot);
    while (iterator.hasNext()) {
        const QString child = iterator.next();
        if (!removePath(child, cancelled))
            return false;
    }
    return QDir(info.absolutePath()).rmdir(info.fileName());
}
}

TrashResult runFileTrash(const QStringList &paths,
                         const std::shared_ptr<std::atomic_bool> &cancelled,
                         const TrashProgress &reportProgress)
{
    TrashResult result;
    QString firstFailure;
    const int total = paths.size();

    for (const QString &rawPath : paths) {
        if (cancelled->load()) {
            result.cancelled = true;
            break;
        }

        const QString path = QDir::cleanPath(QDir::fromNativeSeparators(rawPath));
        reportProgress({int(result.trashedSources.size()), total, path});
        const QFileInfo info(path);
        if (!info.exists() && !info.isSymLink()) {
            result.failedPaths << path;
            if (firstFailure.isEmpty())
                firstFailure = QStringLiteral("“%1” no longer exists.").arg(info.fileName());
            continue;
        }

        QFile item(path);
        if (!item.moveToTrash()) {
            result.failedPaths << path;
            if (firstFailure.isEmpty()) {
                const QString reason = item.errorString();
                firstFailure = reason.isEmpty()
                    ? QStringLiteral("Could not move “%1” to Trash.").arg(info.fileName())
                    : QStringLiteral("Could not move “%1” to Trash: %2")
                          .arg(info.fileName(), reason);
            }
            continue;
        }

        result.trashedSources << path;
        result.trashPaths << item.fileName();
        reportProgress({int(result.trashedSources.size()), total, path});
    }

    const int completed = result.trashedSources.size();
    if (result.cancelled) {
        result.message = completed == 0
            ? QStringLiteral("Trash cancelled.")
            : QStringLiteral("Moved %1 item%2 to Trash before cancelling.")
                  .arg(completed)
                  .arg(completed == 1 ? QString() : QStringLiteral("s"));
        if (!firstFailure.isEmpty())
            result.message += QStringLiteral(" ") + firstFailure;
        return result;
    }

    if (!result.failedPaths.isEmpty()) {
        result.message = QStringLiteral("Moved %1 of %2 items to Trash. %3")
                             .arg(completed)
                             .arg(total)
                             .arg(firstFailure);
        return result;
    }

    result.success = true;
    result.message = QStringLiteral("Moved %1 item%2 to Trash.")
                         .arg(completed)
                         .arg(completed == 1 ? QString() : QStringLiteral("s"));
    return result;
}

TrashResult runEmptyTrash(const QString &filesPath,
                          const QString &infoPath,
                          const std::shared_ptr<std::atomic_bool> &cancelled,
                          const TrashProgress &reportProgress)
{
    TrashResult result;
    const QDir::Filters filters = QDir::AllEntries | QDir::Hidden | QDir::System
        | QDir::NoDotAndDotDot;
    const QFileInfoList items = QDir(filesPath).entryInfoList(filters);
    const QFileInfoList metadata = QDir(infoPath).entryInfoList(filters);
    const int total = items.size() + metadata.size();
    int processed = 0;
    QString firstFailure;

    for (const QFileInfo &item : items) {
        if (cancelled->load()) {
            result.cancelled = true;
            break;
        }
        const QString path = item.absoluteFilePath();
        reportProgress({processed, total, path});
        if (removePath(path, cancelled)) {
            result.trashedSources << path;
        } else {
            if (cancelled->load()) {
                result.cancelled = true;
                break;
            }
            result.failedPaths << path;
            if (firstFailure.isEmpty())
                firstFailure = QStringLiteral("Could not remove “%1” from Trash.")
                                   .arg(item.fileName());
        }
        reportProgress({++processed, total, path});
    }

    if (!result.cancelled) {
        for (const QFileInfo &entry : metadata) {
            if (cancelled->load()) {
                result.cancelled = true;
                break;
            }
            const QString path = entry.absoluteFilePath();
            reportProgress({processed, total, path});

            QString trashName = entry.fileName();
            if (trashName.endsWith(QStringLiteral(".trashinfo")))
                trashName.chop(QStringLiteral(".trashinfo").size());
            const QString matchingItem = QDir(filesPath).filePath(trashName);
            if (!QFileInfo::exists(matchingItem) && !QFileInfo(matchingItem).isSymLink()) {
                if (!removePath(path, cancelled)) {
                    if (cancelled->load()) {
                        result.cancelled = true;
                        break;
                    }
                    result.failedPaths << path;
                    if (firstFailure.isEmpty()) {
                        firstFailure = QStringLiteral("Could not remove Trash metadata for “%1”.")
                                           .arg(trashName);
                    }
                }
            }
            reportProgress({++processed, total, path});
        }
    }

    const int removedItems = result.trashedSources.size();
    if (result.cancelled) {
        result.message = removedItems == 0
            ? QStringLiteral("Empty Trash cancelled.")
            : QStringLiteral("Removed %1 item%2 before cancelling.")
                  .arg(removedItems)
                  .arg(removedItems == 1 ? QString() : QStringLiteral("s"));
        return result;
    }
    if (!result.failedPaths.isEmpty()) {
        result.message = QStringLiteral("Removed %1 item%2 from Trash. %3")
                             .arg(removedItems)
                             .arg(removedItems == 1 ? QString() : QStringLiteral("s"))
                             .arg(firstFailure);
        return result;
    }

    result.success = true;
    result.message = removedItems == 0
        ? QStringLiteral("Trash is already empty.")
        : QStringLiteral("Emptied Trash (%1 item%2).")
              .arg(removedItems)
              .arg(removedItems == 1 ? QString() : QStringLiteral("s"));
    return result;
}
