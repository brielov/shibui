#include "filerestore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUuid>

namespace {
struct RestorePlan
{
    RestoreItem item;
    QString targetPath;
    bool replaceTarget = false;
};

bool pathExists(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() || info.isSymLink();
}

bool validItemName(const QString &name)
{
    return !name.isEmpty() && name != QStringLiteral(".") && name != QStringLiteral("..")
        && !name.contains(QLatin1Char('/'));
}

QString backupPathFor(const QString &targetPath)
{
    const QFileInfo target(targetPath);
    return QDir(target.absolutePath()).filePath(
        QStringLiteral(".%1.shibui-restored-%2")
            .arg(target.fileName(), QUuid::createUuid().toString(QUuid::Id128)));
}

bool removePath(const QString &path)
{
    const QFileInfo info(path);
    if (info.isDir())
        return QDir(path).removeRecursively();
    return !pathExists(path) || QFile::remove(path);
}
}

TransferResult runFileRestore(const QVector<RestoreItem> &items,
                              const std::shared_ptr<std::atomic_bool> &cancelled,
                              const TransferProgress &reportProgress,
                              const TransferConflictResolver &resolveConflict)
{
    TransferResult result;
    QVector<RestorePlan> plans;
    QSet<QString> targets;
    bool applyDecision = false;
    TransferConflictAction remainingAction = TransferConflictAction::Cancel;
    int skippedItems = 0;

    reportProgress({0, items.size(), {}, QStringLiteral("Preparing restore")});
    for (const RestoreItem &item : items) {
        if (cancelled->load()) {
            result.cancelled = true;
            result.message = QStringLiteral("Restore cancelled.");
            return result;
        }

        const QFileInfo source(item.sourcePath);
        if ((!source.exists() && !source.isSymLink()) || item.originalPath.isEmpty()) {
            result.message = QStringLiteral("Trash metadata is missing for “%1”.")
                                 .arg(source.fileName());
            return result;
        }

        const QString originalPath = QDir::cleanPath(item.originalPath);
        const QFileInfo originalParent(QFileInfo(originalPath).absolutePath());
        if (!originalParent.isDir()) {
            result.message = QStringLiteral("The original folder no longer exists: %1")
                                 .arg(originalParent.absoluteFilePath());
            return result;
        }
        if (!originalParent.isWritable()) {
            result.message = QStringLiteral("The original folder is not writable: %1")
                                 .arg(originalParent.absoluteFilePath());
            return result;
        }

        QString targetPath = originalPath;
        bool replaceTarget = false;
        while (pathExists(targetPath) || targets.contains(targetPath)) {
            const bool plannedTarget = targets.contains(targetPath);
            const QFileInfo existing(targetPath);
            const TransferConflictDecision decision = applyDecision
                ? TransferConflictDecision{remainingAction, {}, true}
                : resolveConflict({item.sourcePath, targetPath,
                                   source.isDir() && !source.isSymLink(),
                                   existing.isDir() && !existing.isSymLink()});
            if (cancelled->load() || decision.action == TransferConflictAction::Cancel) {
                result.cancelled = true;
                result.message = QStringLiteral("Restore cancelled.");
                return result;
            }
            if (decision.applyToRemaining
                && (decision.action == TransferConflictAction::Replace
                    || decision.action == TransferConflictAction::Skip)) {
                applyDecision = true;
                remainingAction = decision.action;
            }
            if (decision.action == TransferConflictAction::Skip) {
                ++skippedItems;
                targetPath.clear();
                break;
            }
            if (decision.action == TransferConflictAction::Rename) {
                if (!validItemName(decision.newName)) {
                    result.message = QStringLiteral("The restore name is not valid.");
                    return result;
                }
                targetPath = QDir(originalParent.absoluteFilePath()).filePath(decision.newName);
                continue;
            }
            if (plannedTarget) {
                result.message = QStringLiteral(
                    "Multiple Trash items cannot replace the same destination: %1")
                                     .arg(targetPath);
                return result;
            }
            replaceTarget = true;
            break;
        }
        if (targetPath.isEmpty())
            continue;
        targets.insert(targetPath);
        plans.append({item, targetPath, replaceTarget});
    }

    int completed = 0;
    for (const RestorePlan &plan : plans) {
        if (cancelled->load()) {
            result.cancelled = true;
            result.message = QStringLiteral("Restore cancelled.");
            return result;
        }

        reportProgress({completed, plans.size(), plan.item.sourcePath,
                        QStringLiteral("Restoring")});
        QString backupPath;
        if (plan.replaceTarget && pathExists(plan.targetPath)) {
            backupPath = backupPathFor(plan.targetPath);
            if (!QDir().rename(plan.targetPath, backupPath)) {
                result.message = QStringLiteral("Could not prepare “%1” for replacement.")
                                     .arg(QFileInfo(plan.targetPath).fileName());
                return result;
            }
        }

        if (!QDir().rename(plan.item.sourcePath, plan.targetPath)) {
            if (!backupPath.isEmpty())
                QDir().rename(backupPath, plan.targetPath);
            result.message = QStringLiteral("Could not restore “%1” to %2.")
                                 .arg(QFileInfo(plan.targetPath).fileName(), plan.targetPath);
            return result;
        }

        result.completedSources << plan.item.sourcePath;
        result.destinationPaths << plan.targetPath;
        ++completed;
        reportProgress({completed, plans.size(), plan.item.sourcePath,
                        QStringLiteral("Restoring")});

        if (!QFile::remove(plan.item.infoPath)) {
            result.message = QStringLiteral("Restored “%1”, but could not remove its Trash metadata at %2.")
                                 .arg(QFileInfo(plan.targetPath).fileName(), plan.item.infoPath);
            return result;
        }
        if (!backupPath.isEmpty() && !removePath(backupPath)) {
            result.message = QStringLiteral("Restored “%1”, but could not remove the replaced item at %2.")
                                 .arg(QFileInfo(plan.targetPath).fileName(), backupPath);
            return result;
        }
    }

    result.success = true;
    if (plans.isEmpty()) {
        result.message = QStringLiteral("Skipped %1 item%2.")
                             .arg(skippedItems)
                             .arg(skippedItems == 1 ? QString() : QStringLiteral("s"));
    } else {
        result.message = QStringLiteral("Restored %1 item%2%3.")
                             .arg(plans.size())
                             .arg(plans.size() == 1 ? QString() : QStringLiteral("s"))
                             .arg(skippedItems > 0
                                      ? QStringLiteral("; skipped %1").arg(skippedItems)
                                      : QString());
    }
    return result;
}
