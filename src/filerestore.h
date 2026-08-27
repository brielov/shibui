#pragma once

#include "filetransfer.h"

#include <QVector>

struct RestoreItem
{
    QString sourcePath;
    QString originalPath;
    QString infoPath;
};

TransferResult runFileRestore(const QVector<RestoreItem> &items,
                              const std::shared_ptr<std::atomic_bool> &cancelled,
                              const TransferProgress &reportProgress,
                              const TransferConflictResolver &resolveConflict);
