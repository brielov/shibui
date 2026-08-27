#pragma once

#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

struct TrashUpdate
{
    int completedItems = 0;
    int totalItems = 0;
    QString currentPath;
};

struct TrashResult
{
    bool success = false;
    bool cancelled = false;
    QString message;
    QStringList trashedSources;
    QStringList trashPaths;
    QStringList failedPaths;
};

using TrashProgress = std::function<void(const TrashUpdate &)>;

TrashResult runFileTrash(const QStringList &paths,
                         const std::shared_ptr<std::atomic_bool> &cancelled,
                         const TrashProgress &reportProgress);

TrashResult runEmptyTrash(const QString &filesPath,
                          const QString &infoPath,
                          const std::shared_ptr<std::atomic_bool> &cancelled,
                          const TrashProgress &reportProgress);
