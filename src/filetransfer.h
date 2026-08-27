#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

struct TransferUpdate
{
    qint64 completedWork = 0;
    qint64 totalWork = 0;
    QString currentPath;
    QString phase;
};

struct TransferResult
{
    bool success = false;
    bool cancelled = false;
    QString message;
    QStringList completedSources;
    QStringList destinationPaths;
    QStringList replacedTargetPaths;
    QStringList replacedTrashPaths;
    QStringList replacedTrashInfoPaths;
};

struct UndoMoveItem
{
    QString currentPath;
    QString originalPath;
    QString replacedTargetPath;
    QString replacedTrashPath;
    QString replacedTrashInfoPath;
};

enum class TransferConflictAction
{
    Replace,
    Skip,
    Rename,
    Cancel,
};

struct TransferConflict
{
    QString sourcePath;
    QString targetPath;
    bool sourceIsDirectory = false;
    bool targetIsDirectory = false;
};

struct TransferConflictDecision
{
    TransferConflictAction action = TransferConflictAction::Cancel;
    QString newName;
    bool applyToRemaining = false;
};

using TransferProgress = std::function<void(const TransferUpdate &)>;
using TransferConflictResolver = std::function<TransferConflictDecision(const TransferConflict &)>;

TransferResult runFileTransfer(const QStringList &sources,
                               const QString &destinationDirectory,
                               bool move,
                               const std::shared_ptr<std::atomic_bool> &cancelled,
                               const TransferProgress &reportProgress,
                               const TransferConflictResolver &resolveConflict = {});

TransferResult runMoveUndo(const QVector<UndoMoveItem> &items,
                           const std::shared_ptr<std::atomic_bool> &cancelled,
                           const TransferProgress &reportProgress,
                           const QString &createdContainerPath = QString());
