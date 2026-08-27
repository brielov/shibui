#include "filetransfer.h"

#include <QDir>
#include <QDirIterator>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryFile>
#include <QUuid>
#include <QVector>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

namespace {
struct FileIdentity
{
    quint64 device = 0;
    quint64 inode = 0;
    quint64 size = 0;
    qint64 accessedSeconds = 0;
    qint64 accessedNanoseconds = 0;
    qint64 modifiedSeconds = 0;
    qint64 modifiedNanoseconds = 0;
    quint32 mode = 0;
    bool valid = false;
};

struct TransferPlan
{
    QString sourcePath;
    QString targetPath;
    qint64 work = 0;
    bool replaceTarget = false;
};

QString cleanPath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QString resolvedPath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty())
        return cleanPath(canonical);

    const QString canonicalParent = QFileInfo(info.absolutePath()).canonicalFilePath();
    return canonicalParent.isEmpty()
        ? cleanPath(info.absoluteFilePath())
        : cleanPath(QDir(canonicalParent).filePath(info.fileName()));
}

bool pathIsInside(const QString &path, const QString &directory)
{
    const QString resolvedDirectory = resolvedPath(directory);
    const QString resolvedCandidate = resolvedPath(path);
    return resolvedCandidate == resolvedDirectory
        || resolvedCandidate.startsWith(resolvedDirectory + QLatin1Char('/'));
}

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

QString replacementBackupPath(const QString &targetPath)
{
    const QFileInfo target(targetPath);
    const QString backupName = QStringLiteral(".%1.shibui-replaced-%2")
                                   .arg(target.fileName(),
                                        QUuid::createUuid().toString(QUuid::Id128));
    return cleanPath(QDir(target.absolutePath()).absoluteFilePath(backupName));
}

QString uniqueCopyPath(const QString &sourcePath, const QString &destinationDirectory)
{
    const QFileInfo source(sourcePath);
    const QString name = source.fileName();
    QString stem = name;
    QString extension;
    if (!source.isDir()) {
        const int dot = name.lastIndexOf(QLatin1Char('.'));
        if (dot > 0) {
            stem = name.left(dot);
            extension = name.mid(dot);
        }
    }

    QDir destination(destinationDirectory);
    for (int number = 1; ; ++number) {
        const QString suffix = number == 1
            ? QStringLiteral(" (copy)")
            : QStringLiteral(" (copy %1)").arg(number);
        const QString candidate = destination.absoluteFilePath(stem + suffix + extension);
        if (!pathExists(candidate))
            return cleanPath(candidate);
    }
}

bool removeCreatedPath(const QString &path)
{
    const QFileInfo info(path);
    if (info.isDir())
        return QDir(path).removeRecursively();
    return !pathExists(path) || QFile::remove(path);
}

bool readIdentity(const QString &path, FileIdentity &identity)
{
    struct stat status {};
    if (::lstat(QFile::encodeName(path).constData(), &status) != 0) {
        identity = {};
        return false;
    }
    identity.device = static_cast<quint64>(status.st_dev);
    identity.inode = static_cast<quint64>(status.st_ino);
    identity.size = static_cast<quint64>(qMax<qint64>(0, status.st_size));
    identity.accessedSeconds = status.st_atim.tv_sec;
    identity.accessedNanoseconds = status.st_atim.tv_nsec;
    identity.modifiedSeconds = status.st_mtim.tv_sec;
    identity.modifiedNanoseconds = status.st_mtim.tv_nsec;
    identity.mode = static_cast<quint32>(status.st_mode);
    identity.valid = true;
    return true;
}

bool sameNode(const FileIdentity &left, const FileIdentity &right)
{
    return left.valid && right.valid
        && left.device == right.device
        && left.inode == right.inode
        && (left.mode & S_IFMT) == (right.mode & S_IFMT);
}

bool sameSnapshot(const FileIdentity &left, const FileIdentity &right)
{
    return sameNode(left, right)
        && left.size == right.size
        && left.modifiedSeconds == right.modifiedSeconds
        && left.modifiedNanoseconds == right.modifiedNanoseconds;
}

bool supportedSource(const QFileInfo &info)
{
    return info.isSymLink() || info.isDir() || info.isFile();
}

void writeRemovalRecord(QDataStream &stream, const QString &path,
                        const FileIdentity &identity)
{
    stream << path << identity.device << identity.inode << identity.size
           << identity.accessedSeconds << identity.accessedNanoseconds
           << identity.modifiedSeconds << identity.modifiedNanoseconds << identity.mode;
}

bool readRemovalRecord(QDataStream &stream, QString &path, FileIdentity &identity)
{
    if (stream.atEnd())
        return false;
    stream >> path >> identity.device >> identity.inode >> identity.size
           >> identity.accessedSeconds >> identity.accessedNanoseconds
           >> identity.modifiedSeconds >> identity.modifiedNanoseconds >> identity.mode;
    identity.valid = stream.status() == QDataStream::Ok;
    return identity.valid;
}

bool removeRecordedEntries(QTemporaryFile &manifest, QString &error)
{
    manifest.flush();
    if (!manifest.seek(0)) {
        error += QStringLiteral(" Could not read the transfer cleanup manifest.");
        return false;
    }

    QDataStream stream(&manifest);
    QString path;
    FileIdentity expected;
    while (readRemovalRecord(stream, path, expected)) {
        FileIdentity current;
        if (!readIdentity(path, current))
            continue;
        const bool directory = S_ISDIR(expected.mode);
        if (!sameNode(current, expected)
            || (!directory && !sameSnapshot(current, expected))) {
            error += QStringLiteral(" Refusing to remove an item changed during the transfer: %1.")
                         .arg(path);
            return false;
        }

        const QFileInfo info(path);
        const bool removed = directory
            ? QDir(info.absolutePath()).rmdir(info.fileName())
            : QFile::remove(path);
        if (!removed) {
            error += directory
                ? QStringLiteral(" Folder contains new or changed items: %1.").arg(path)
                : QStringLiteral(" Could not remove the copied source item: %1.").arg(path);
            return false;
        }
    }
    if (stream.status() != QDataStream::Ok && stream.status() != QDataStream::ReadPastEnd) {
        error += QStringLiteral(" The transfer cleanup manifest is invalid.");
        return false;
    }
    return true;
}

bool restoreReplacement(const QString &targetPath, const QString &backupPath, QString &error)
{
    if (pathExists(targetPath)) {
        error += QStringLiteral(" The original item remains at %1 because the destination is occupied.")
                     .arg(backupPath);
        return false;
    }
    if (!backupPath.isEmpty() && pathExists(backupPath)
        && !QDir().rename(backupPath, targetPath)) {
        error += QStringLiteral(" The original item remains at %1.").arg(backupPath);
        return false;
    }
    return true;
}

bool trashReplacement(const QString &targetPath,
                      QString &trashPath,
                      QString &trashInfoPath,
                      QString &error)
{
    QFile target(targetPath);
    if (!target.moveToTrash()) {
        error = target.errorString().isEmpty()
            ? QStringLiteral("Could not move the existing item to Trash: %1").arg(targetPath)
            : QStringLiteral("Could not move the existing item to Trash: %1")
                  .arg(target.errorString());
        return false;
    }
    trashPath = target.fileName();
    const QString trashRoot = QFileInfo(QFileInfo(trashPath).absolutePath()).absolutePath();
    trashInfoPath = QDir(trashRoot).filePath(
        QStringLiteral("info/%1.trashinfo").arg(QFileInfo(trashPath).fileName()));
    return true;
}

bool restoreTrashedReplacement(const QString &targetPath,
                               const QString &trashPath,
                               const QString &trashInfoPath,
                               QString &error)
{
    if (pathExists(targetPath)) {
        error += QStringLiteral(" The replaced item remains in Trash because %1 is occupied.")
                     .arg(targetPath);
        return false;
    }
    if (!QDir().rename(trashPath, targetPath)) {
        error += QStringLiteral(" The replaced item remains in Trash at %1.").arg(trashPath);
        return false;
    }
    if (QFileInfo::exists(trashInfoPath) && !QFile::remove(trashInfoPath)) {
        error += QStringLiteral(" Could not remove Trash metadata at %1.").arg(trashInfoPath);
        return false;
    }
    return true;
}

bool copySymbolicLink(const QString &sourcePath, const QString &targetPath, QString &error)
{
    const QByteArray encodedSource = QFile::encodeName(sourcePath);
    QByteArray linkTarget(256, Qt::Uninitialized);
    for (;;) {
        const ssize_t length = ::readlink(encodedSource.constData(), linkTarget.data(), linkTarget.size());
        if (length < 0) {
            error = QStringLiteral("Could not read symbolic link %1: %2")
                        .arg(sourcePath, QString::fromLocal8Bit(std::strerror(errno)));
            return false;
        }
        if (length < linkTarget.size()) {
            linkTarget.resize(length);
            break;
        }
        linkTarget.resize(linkTarget.size() * 2);
    }

    const QByteArray encodedTarget = QFile::encodeName(targetPath);
    if (::symlink(linkTarget.constData(), encodedTarget.constData()) != 0) {
        error = QStringLiteral("Could not copy symbolic link %1: %2")
                    .arg(sourcePath, QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    return true;
}

bool copyExtendedAttributes(const QString &sourcePath, const QString &targetPath, QString &error)
{
    const QByteArray source = QFile::encodeName(sourcePath);
    const QByteArray target = QFile::encodeName(targetPath);
    ssize_t namesSize = ::llistxattr(source.constData(), nullptr, 0);
    if (namesSize < 0) {
        if (errno == ENOTSUP || errno == EOPNOTSUPP)
            return true;
        error = QStringLiteral("Could not read metadata for %1: %2")
                    .arg(sourcePath, QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    if (namesSize == 0)
        return true;

    QByteArray names(namesSize, Qt::Uninitialized);
    namesSize = ::llistxattr(source.constData(), names.data(), names.size());
    if (namesSize < 0) {
        error = QStringLiteral("Could not read metadata for %1: %2")
                    .arg(sourcePath, QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }

    for (qsizetype offset = 0; offset < namesSize;) {
        const char *name = names.constData() + offset;
        const qsizetype nameSize = qstrnlen(name, namesSize - offset);
        if (nameSize == 0 || offset + nameSize >= namesSize) {
            error = QStringLiteral("Could not read metadata for %1: invalid attribute list.")
                        .arg(sourcePath);
            return false;
        }

        ssize_t valueSize = ::lgetxattr(source.constData(), name, nullptr, 0);
        if (valueSize < 0) {
            error = QStringLiteral("Could not read metadata for %1: %2")
                        .arg(sourcePath, QString::fromLocal8Bit(std::strerror(errno)));
            return false;
        }
        QByteArray value(valueSize, Qt::Uninitialized);
        if (valueSize > 0) {
            const ssize_t readSize = ::lgetxattr(source.constData(), name,
                                                 value.data(), value.size());
            if (readSize != valueSize) {
                error = QStringLiteral("Metadata changed while copying %1.").arg(sourcePath);
                return false;
            }
        }
        if (::lsetxattr(target.constData(), name, value.constData(), value.size(), 0) != 0) {
            error = QStringLiteral("Could not preserve metadata for %1: %2")
                        .arg(targetPath, QString::fromLocal8Bit(std::strerror(errno)));
            return false;
        }
        offset += nameSize + 1;
    }
    return true;
}

bool preserveMetadata(const QString &sourcePath, const QString &targetPath,
                      const FileIdentity &sourceIdentity, bool symbolicLink,
                      QString &error)
{
    const QByteArray target = QFile::encodeName(targetPath);
    if (!symbolicLink && ::chmod(target.constData(), sourceIdentity.mode & 07777) != 0) {
        error = QStringLiteral("Could not preserve permissions for %1: %2")
                    .arg(targetPath, QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    if (!copyExtendedAttributes(sourcePath, targetPath, error))
        return false;

    const struct timespec times[] = {
        {sourceIdentity.accessedSeconds, sourceIdentity.accessedNanoseconds},
        {sourceIdentity.modifiedSeconds, sourceIdentity.modifiedNanoseconds},
    };
    if (::utimensat(AT_FDCWD, target.constData(), times,
                    symbolicLink ? AT_SYMLINK_NOFOLLOW : 0) != 0) {
        error = QStringLiteral("Could not preserve timestamps for %1: %2")
                    .arg(targetPath, QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    return true;
}

bool calculateWork(const QString &path,
                   const std::shared_ptr<std::atomic_bool> &cancelled,
                   qint64 &work,
                   QString &error)
{
    if (cancelled->load())
        return false;
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) {
        error = QStringLiteral("Source no longer exists: %1").arg(path);
        return false;
    }
    if (!supportedSource(info)) {
        error = QStringLiteral("Unsupported file type: %1").arg(path);
        return false;
    }
    if (info.isDir() && !info.isSymLink() && !info.isReadable()) {
        error = QStringLiteral("Could not read folder: %1").arg(path);
        return false;
    }

    work += (!info.isDir() && !info.isSymLink()) ? qMax<qint64>(1, info.size()) : 1;
    if (!info.isDir() || info.isSymLink())
        return true;

    QDirIterator iterator(path,
                          QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    while (iterator.hasNext()) {
        const QString childPath = iterator.next();
        if (!calculateWork(childPath, cancelled, work, error))
            return false;
    }
    return true;
}

bool copyEntry(const QString &sourcePath,
               const QString &targetPath,
               qint64 &completedWork,
               qint64 totalWork,
               const std::shared_ptr<std::atomic_bool> &cancelled,
               const TransferProgress &reportProgress,
               QDataStream &sourceRemoval,
               QDataStream &targetRemoval,
               FileIdentity *rootTargetIdentity,
               QString &error)
{
    constexpr qint64 chunkSize = 4 * 1024 * 1024;
    if (cancelled->load())
        return false;

    const QFileInfo sourceInfo(sourcePath);
    if ((!sourceInfo.exists() && !sourceInfo.isSymLink()) || !supportedSource(sourceInfo)) {
        error = supportedSource(sourceInfo)
            ? QStringLiteral("Source no longer exists: %1").arg(sourcePath)
            : QStringLiteral("Unsupported file type: %1").arg(sourcePath);
        return false;
    }
    FileIdentity sourceBefore;
    if (!readIdentity(sourcePath, sourceBefore)) {
        error = QStringLiteral("Could not inspect source: %1").arg(sourcePath);
        return false;
    }

    reportProgress({completedWork, totalWork, sourcePath, QStringLiteral("Copying")});
    auto recordTarget = [&] {
        FileIdentity targetIdentity;
        if (!readIdentity(targetPath, targetIdentity))
            return false;
        writeRemovalRecord(targetRemoval, targetPath, targetIdentity);
        if (rootTargetIdentity && !rootTargetIdentity->valid)
            *rootTargetIdentity = targetIdentity;
        return true;
    };

    if (sourceInfo.isSymLink()) {
        if (!copySymbolicLink(sourcePath, targetPath, error))
            return false;
        if (!preserveMetadata(sourcePath, targetPath, sourceBefore, true, error)) {
            recordTarget();
            return false;
        }
        if (!recordTarget()) {
            error = QStringLiteral("Could not inspect copied symbolic link: %1").arg(targetPath);
            return false;
        }
        FileIdentity sourceAfter;
        if (!readIdentity(sourcePath, sourceAfter) || !sameSnapshot(sourceBefore, sourceAfter)) {
            error = QStringLiteral("Source changed while it was copied: %1").arg(sourcePath);
            return false;
        }
        writeRemovalRecord(sourceRemoval, sourcePath, sourceAfter);
        ++completedWork;
        return true;
    }

    if (sourceInfo.isDir()) {
        const QFileInfo targetInfo(targetPath);
        if (!QDir(targetInfo.absolutePath()).mkdir(targetInfo.fileName())) {
            error = QStringLiteral("Could not create folder: %1").arg(targetPath);
            return false;
        }
        if (rootTargetIdentity && !rootTargetIdentity->valid)
            readIdentity(targetPath, *rootTargetIdentity);

        QDirIterator iterator(sourcePath,
                              QDir::AllEntries | QDir::Hidden | QDir::System
                                  | QDir::NoDotAndDotDot);
        while (iterator.hasNext()) {
            const QString childSource = iterator.next();
            const QString childTarget = QDir(targetPath).filePath(QFileInfo(childSource).fileName());
            if (!copyEntry(childSource, childTarget, completedWork, totalWork, cancelled,
                           reportProgress, sourceRemoval, targetRemoval, nullptr, error)) {
                recordTarget();
                return false;
            }
        }

        if (!preserveMetadata(sourcePath, targetPath, sourceBefore, false, error)) {
            recordTarget();
            return false;
        }
        FileIdentity sourceAfter;
        if (!readIdentity(sourcePath, sourceAfter) || !sameSnapshot(sourceBefore, sourceAfter)) {
            recordTarget();
            error = QStringLiteral("Folder changed while it was copied: %1").arg(sourcePath);
            return false;
        }
        if (!recordTarget()) {
            error = QStringLiteral("Could not inspect copied folder: %1").arg(targetPath);
            return false;
        }
        writeRemovalRecord(sourceRemoval, sourcePath, sourceAfter);
        ++completedWork;
        return true;
    }

    QFile source(sourcePath);
        if (!source.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Could not read %1: %2").arg(sourcePath, source.errorString());
            return false;
        }
        QFile target(targetPath);
        if (!target.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            error = QStringLiteral("Could not create %1: %2").arg(targetPath, target.errorString());
            return false;
        }
        if (rootTargetIdentity && !rootTargetIdentity->valid)
            readIdentity(targetPath, *rootTargetIdentity);

        qint64 fileCompleted = 0;
        while (!source.atEnd()) {
            if (cancelled->load()) {
                target.close();
                recordTarget();
                return false;
            }
            const QByteArray data = source.read(chunkSize);
            if (data.isEmpty() && source.error() != QFileDevice::NoError) {
                error = QStringLiteral("Could not read %1: %2").arg(sourcePath, source.errorString());
                target.close();
                recordTarget();
                return false;
            }
            qint64 written = 0;
            while (written < data.size()) {
                const qint64 count = target.write(data.constData() + written, data.size() - written);
                if (count <= 0) {
                    error = QStringLiteral("Could not write %1: %2").arg(targetPath,
                                                                         target.errorString());
                    target.close();
                    recordTarget();
                    return false;
                }
                written += count;
            }
            fileCompleted += data.size();
            reportProgress({completedWork + qMin(fileCompleted, qint64(sourceBefore.size)),
                            totalWork, sourcePath, QStringLiteral("Copying")});
        }
        if (fileCompleted != qint64(sourceBefore.size)) {
            error = QStringLiteral("Could not finish reading %1: the source changed or disconnected.")
                        .arg(sourcePath);
            target.close();
            recordTarget();
            return false;
        }
        if (!target.flush()) {
            error = QStringLiteral("Could not finish writing %1: %2")
                        .arg(targetPath, target.errorString());
            target.close();
            recordTarget();
            return false;
        }
        target.close();
        if (!preserveMetadata(sourcePath, targetPath, sourceBefore, false, error)) {
            recordTarget();
            return false;
        }
        if (!recordTarget()) {
            error = QStringLiteral("Could not inspect copied file: %1").arg(targetPath);
            return false;
        }
        FileIdentity sourceAfter;
        if (!readIdentity(sourcePath, sourceAfter) || !sameSnapshot(sourceBefore, sourceAfter)) {
            error = QStringLiteral("Source changed while it was copied: %1").arg(sourcePath);
            return false;
        }
        writeRemovalRecord(sourceRemoval, sourcePath, sourceAfter);
        completedWork += qMax<qint64>(1, fileCompleted);
        return true;
}

bool copyPlan(const TransferPlan &plan,
              qint64 &completedWork,
              qint64 totalWork,
              const std::shared_ptr<std::atomic_bool> &cancelled,
              const TransferProgress &reportProgress,
              QTemporaryFile &sourceRemovalManifest,
              QTemporaryFile &targetRemovalManifest,
              FileIdentity &targetIdentity,
              QString &error)
{
    QDataStream sourceRemoval(&sourceRemovalManifest);
    QDataStream targetRemoval(&targetRemovalManifest);
    if (!copyEntry(plan.sourcePath, plan.targetPath, completedWork, totalWork, cancelled,
                   reportProgress, sourceRemoval, targetRemoval, &targetIdentity, error)) {
        sourceRemovalManifest.flush();
        targetRemovalManifest.flush();
        return false;
    }
    sourceRemovalManifest.flush();
    targetRemovalManifest.flush();
    return true;
}
}

TransferResult runFileTransfer(const QStringList &sources,
                               const QString &destinationDirectory,
                               bool move,
                               const std::shared_ptr<std::atomic_bool> &cancelled,
                               const TransferProgress &reportProgress,
                               const TransferConflictResolver &resolveConflict)
{
    TransferResult result;
    QVector<TransferPlan> plans;
    QSet<QString> targets;
    const QString destinationPath = cleanPath(destinationDirectory);
    bool applyDecision = false;
    TransferConflictAction remainingAction = TransferConflictAction::Cancel;
    int skippedItems = 0;

    reportProgress({0, 0, {}, QStringLiteral("Preparing")});
    for (const QString &rawSource : sources) {
        if (cancelled->load()) {
            result.cancelled = true;
            result.message = QStringLiteral("Transfer cancelled.");
            return result;
        }

        const QString sourcePath = cleanPath(rawSource);
        const QFileInfo source(sourcePath);
        if (!source.exists() && !source.isSymLink()) {
            result.message = QStringLiteral("Source no longer exists: %1").arg(sourcePath);
            return result;
        }
        if (source.isDir() && !source.isSymLink()
            && (destinationPath == sourcePath || pathIsInside(destinationPath, sourcePath))) {
            result.message = QStringLiteral("Cannot place a folder inside itself: %1").arg(source.fileName());
            return result;
        }

        QString targetPath = cleanPath(QDir(destinationPath).absoluteFilePath(source.fileName()));
        bool replaceTarget = false;
        if (!move && cleanPath(source.absolutePath()) == destinationPath) {
            targetPath = uniqueCopyPath(sourcePath, destinationPath);
        } else if (move && sourcePath == targetPath) {
            result.message = QStringLiteral("“%1” is already in the destination.").arg(source.fileName());
            return result;
        } else {
            while (pathExists(targetPath) || targets.contains(targetPath)) {
                const bool plannedTarget = targets.contains(targetPath);
                if (!resolveConflict) {
                    result.message = QStringLiteral("An item named “%1” already exists in the destination.")
                                         .arg(QFileInfo(targetPath).fileName());
                    return result;
                }

                const QFileInfo existingTarget(targetPath);
                const TransferConflictDecision decision = applyDecision
                    ? TransferConflictDecision{remainingAction, {}, true}
                    : resolveConflict({sourcePath, targetPath, source.isDir() && !source.isSymLink(),
                                       existingTarget.isDir() && !existingTarget.isSymLink()});
                if (cancelled->load() || decision.action == TransferConflictAction::Cancel) {
                    result.cancelled = true;
                    result.message = QStringLiteral("Transfer cancelled.");
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
                        result.message = QStringLiteral("The replacement name is not valid.");
                        return result;
                    }
                    targetPath = cleanPath(QDir(destinationPath).absoluteFilePath(decision.newName));
                    continue;
                }

                if (plannedTarget) {
                    result.message = QStringLiteral(
                        "Multiple selected items cannot replace the same destination: %1")
                                         .arg(targetPath);
                    return result;
                }

                replaceTarget = true;
                break;
            }
        }
        if (targetPath.isEmpty())
            continue;
        targets.insert(targetPath);
        plans.append({sourcePath, targetPath, 0, replaceTarget});
    }

    QString error;
    qint64 totalWork = 0;
    for (TransferPlan &plan : plans) {
        if (!calculateWork(plan.sourcePath, cancelled, plan.work, error)) {
            result.cancelled = cancelled->load();
            result.message = result.cancelled ? QStringLiteral("Transfer cancelled.") : error;
            return result;
        }
        totalWork += plan.work;
    }

    qint64 completedWork = 0;
    reportProgress({0, totalWork, {}, move ? QStringLiteral("Moving") : QStringLiteral("Copying")});
    for (const TransferPlan &plan : plans) {
        if (cancelled->load()) {
            result.cancelled = true;
            result.message = QStringLiteral("Transfer cancelled.");
            return result;
        }

        QString backupPath;
        QString replacedTrashPath;
        QString replacedTrashInfoPath;
        if (plan.replaceTarget && pathExists(plan.targetPath)) {
            bool prepared = false;
            if (move) {
                prepared = trashReplacement(plan.targetPath, replacedTrashPath,
                                            replacedTrashInfoPath, error);
            } else {
                backupPath = replacementBackupPath(plan.targetPath);
                prepared = QDir().rename(plan.targetPath, backupPath);
            }
            if (!prepared) {
                result.message = QStringLiteral("Could not prepare “%1” for replacement.")
                                     .arg(QFileInfo(plan.targetPath).fileName());
                if (!error.isEmpty())
                    result.message += QStringLiteral(" ") + error;
                return result;
            }
        }

        if (move && QDir().rename(plan.sourcePath, plan.targetPath)) {
            completedWork += plan.work;
            result.completedSources << plan.sourcePath;
            result.destinationPaths << plan.targetPath;
            result.replacedTargetPaths << (replacedTrashPath.isEmpty() ? QString()
                                                                       : plan.targetPath);
            result.replacedTrashPaths << replacedTrashPath;
            result.replacedTrashInfoPaths << replacedTrashInfoPath;
            reportProgress({completedWork, totalWork, plan.sourcePath, QStringLiteral("Moving")});
            if (!backupPath.isEmpty() && !removeCreatedPath(backupPath)) {
                result.message = QStringLiteral("Replaced “%1”, but could not remove the backup at %2.")
                                     .arg(QFileInfo(plan.targetPath).fileName(), backupPath);
                return result;
            }
            continue;
        }

        if (QFileInfo(plan.sourcePath).isDir()
            && pathIsInside(plan.targetPath, plan.sourcePath)) {
            result.message = QStringLiteral("Cannot place a folder inside itself: %1")
                                 .arg(QFileInfo(plan.sourcePath).fileName());
            return result;
        }

        QTemporaryFile sourceRemovalManifest;
        QTemporaryFile targetRemovalManifest;
        if (!sourceRemovalManifest.open() || !targetRemovalManifest.open()) {
            result.message = QStringLiteral("Could not prepare transfer cleanup data.");
            return result;
        }
        const qint64 beforePlan = completedWork;
        FileIdentity targetIdentity;
        if (!copyPlan(plan, completedWork, totalWork, cancelled, reportProgress,
                      sourceRemovalManifest, targetRemovalManifest, targetIdentity, error)) {
            QString cleanupError;
            const bool targetRemoved = removeRecordedEntries(targetRemovalManifest, cleanupError)
                && !pathExists(plan.targetPath);
            error += cleanupError;
            if (targetRemoved) {
                if (!replacedTrashPath.isEmpty())
                    restoreTrashedReplacement(plan.targetPath, replacedTrashPath,
                                               replacedTrashInfoPath, error);
                else if (!backupPath.isEmpty())
                    restoreReplacement(plan.targetPath, backupPath, error);
            } else if (!replacedTrashPath.isEmpty() || !backupPath.isEmpty()) {
                error += QStringLiteral(" The replaced item was preserved separately because cleanup was incomplete.");
            }
            completedWork = beforePlan;
            result.cancelled = cancelled->load();
            result.message = result.cancelled
                ? QStringLiteral("Transfer cancelled.%1")
                      .arg(error.isEmpty() ? QString() : QStringLiteral(" ") + error)
                : error;
            return result;
        }
        if (!pathExists(plan.targetPath)) {
            result.message = QStringLiteral("The destination disappeared before the transfer finished: %1")
                                 .arg(plan.targetPath);
            return result;
        }
        FileIdentity currentTargetIdentity;
        if (!readIdentity(plan.targetPath, currentTargetIdentity)
            || !sameNode(currentTargetIdentity, targetIdentity)
            || (QFileInfo(plan.sourcePath).isDir()
                && pathIsInside(plan.targetPath, plan.sourcePath))) {
            result.message = QStringLiteral(
                "The destination changed before the transfer could be finalized: %1")
                                 .arg(plan.targetPath);
            return result;
        }
        if (move && !removeRecordedEntries(sourceRemovalManifest, error)) {
            error = QStringLiteral("Copied “%1” but could not remove the source.")
                        .arg(QFileInfo(plan.sourcePath).fileName()) + error;
            result.message = error;
            return result;
        }

        result.completedSources << plan.sourcePath;
        result.destinationPaths << plan.targetPath;
        result.replacedTargetPaths << (replacedTrashPath.isEmpty() ? QString()
                                                                   : plan.targetPath);
        result.replacedTrashPaths << replacedTrashPath;
        result.replacedTrashInfoPaths << replacedTrashInfoPath;
        if (!backupPath.isEmpty() && !removeCreatedPath(backupPath)) {
            result.message = QStringLiteral("Replaced “%1”, but could not remove the backup at %2.")
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
        result.message = QStringLiteral("%1 %2 item%3%4.")
                             .arg(move ? QStringLiteral("Moved") : QStringLiteral("Copied"))
                             .arg(plans.size())
                             .arg(plans.size() == 1 ? QString() : QStringLiteral("s"))
                             .arg(skippedItems > 0
                                      ? QStringLiteral("; skipped %1").arg(skippedItems)
                                      : QString());
    }
    return result;
}

TransferResult runMoveUndo(const QVector<UndoMoveItem> &items,
                           const std::shared_ptr<std::atomic_bool> &cancelled,
                           const TransferProgress &reportProgress,
                           const QString &createdContainerPath)
{
    TransferResult result;
    QVector<TransferPlan> plans;
    QSet<QString> targets;
    QString error;
    qint64 totalWork = 0;

    if (!createdContainerPath.isEmpty()) {
        QSet<QString> expectedPaths;
        for (const UndoMoveItem &item : items)
            expectedPaths.insert(cleanPath(item.currentPath));
        const QFileInfo container(createdContainerPath);
        if (!container.isDir()) {
            result.message = QStringLiteral(
                "Cannot undo: the folder created for the selection no longer exists.");
            return result;
        }
        const QFileInfoList contents = QDir(createdContainerPath).entryInfoList(
            QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : contents) {
            if (!expectedPaths.contains(cleanPath(entry.absoluteFilePath()))) {
                result.message = QStringLiteral(
                    "Cannot undo: the new folder contains an item that was added later: %1.")
                                     .arg(entry.fileName());
                return result;
            }
        }
    }

    reportProgress({0, 0, {}, QStringLiteral("Preparing undo")});
    for (const UndoMoveItem &item : items) {
        if (cancelled->load()) {
            result.cancelled = true;
            result.message = QStringLiteral("Undo cancelled.");
            return result;
        }

        const QString currentPath = cleanPath(item.currentPath);
        const QString originalPath = cleanPath(item.originalPath);
        const QFileInfo current(currentPath);
        const QFileInfo originalParent(QFileInfo(originalPath).absolutePath());
        if (!current.exists() && !current.isSymLink()) {
            result.message = QStringLiteral("Cannot undo: the moved item no longer exists at %1.")
                                 .arg(currentPath);
            return result;
        }
        if (pathExists(originalPath) || targets.contains(originalPath)) {
            result.message = QStringLiteral("Cannot undo: %1 is no longer available.")
                                 .arg(originalPath);
            return result;
        }
        if (!originalParent.isDir() || !originalParent.isWritable()) {
            result.message = QStringLiteral("Cannot undo: the original folder is unavailable or not writable: %1.")
                                 .arg(originalParent.absoluteFilePath());
            return result;
        }
        if (!item.replacedTrashPath.isEmpty()
            && !pathExists(item.replacedTrashPath)) {
            result.message = QStringLiteral("Cannot undo: the replaced item is no longer in Trash.");
            return result;
        }

        TransferPlan plan{currentPath, originalPath, 0, false};
        if (!calculateWork(plan.sourcePath, cancelled, plan.work, error)) {
            result.cancelled = cancelled->load();
            result.message = result.cancelled ? QStringLiteral("Undo cancelled.") : error;
            return result;
        }
        targets.insert(originalPath);
        totalWork += plan.work;
        plans.append(std::move(plan));
    }

    qint64 completedWork = 0;
    reportProgress({0, totalWork, {}, QStringLiteral("Undoing move")});
    for (int index = 0; index < plans.size(); ++index) {
        if (cancelled->load()) {
            result.cancelled = true;
            result.message = QStringLiteral("Undo cancelled after restoring %1 item%2.")
                                 .arg(result.completedSources.size())
                                 .arg(result.completedSources.size() == 1
                                          ? QString() : QStringLiteral("s"));
            return result;
        }

        const TransferPlan &plan = plans.at(index);
        const UndoMoveItem &item = items.at(index);
        bool moved = QDir().rename(plan.sourcePath, plan.targetPath);
        if (moved) {
            completedWork += plan.work;
        } else {
            const qint64 beforePlan = completedWork;
            QTemporaryFile sourceRemovalManifest;
            QTemporaryFile targetRemovalManifest;
            if (!sourceRemovalManifest.open() || !targetRemovalManifest.open()) {
                result.message = QStringLiteral("Could not prepare undo cleanup data.");
                return result;
            }
            FileIdentity targetIdentity;
            if (!copyPlan(plan, completedWork, totalWork, cancelled, reportProgress,
                          sourceRemovalManifest, targetRemovalManifest, targetIdentity, error)) {
                QString cleanupError;
                removeRecordedEntries(targetRemovalManifest, cleanupError);
                error += cleanupError;
                completedWork = beforePlan;
                result.cancelled = cancelled->load();
                result.message = result.cancelled ? QStringLiteral("Undo cancelled.") : error;
                return result;
            }
            FileIdentity currentTargetIdentity;
            if (!readIdentity(plan.targetPath, currentTargetIdentity)
                || !sameNode(currentTargetIdentity, targetIdentity)
                || !removeRecordedEntries(sourceRemovalManifest, error)) {
                result.message = QStringLiteral("Cannot undo: copied the item back but could not remove %1.")
                                     .arg(plan.sourcePath) + error;
                return result;
            }
        }

        result.completedSources << plan.sourcePath;
        result.destinationPaths << plan.targetPath;
        reportProgress({completedWork, totalWork, plan.sourcePath,
                        QStringLiteral("Undoing move")});

        if (!item.replacedTrashPath.isEmpty()
            && !restoreTrashedReplacement(item.replacedTargetPath,
                                           item.replacedTrashPath,
                                           item.replacedTrashInfoPath, error)) {
            result.message = QStringLiteral("Moved the item back, but could not restore the item it replaced.%1")
                                 .arg(error);
            return result;
        }
    }

    if (!createdContainerPath.isEmpty()) {
        const QFileInfo container(createdContainerPath);
        if (!container.isDir()
            || !QDir(createdContainerPath)
                    .entryList(QDir::AllEntries | QDir::Hidden | QDir::System
                               | QDir::NoDotAndDotDot).isEmpty()
            || !QDir(container.absolutePath()).rmdir(container.fileName())) {
            result.message = QStringLiteral(
                "Restored the items, but could not remove the folder created for them: %1.")
                                 .arg(createdContainerPath);
            return result;
        }
    }

    result.success = true;
    result.message = createdContainerPath.isEmpty()
        ? QStringLiteral("Undid move of %1 item%2.")
              .arg(items.size()).arg(items.size() == 1 ? QString() : QStringLiteral("s"))
        : QStringLiteral("Restored %1 item%2 and removed the folder created for them.")
              .arg(items.size()).arg(items.size() == 1 ? QString() : QStringLiteral("s"));
    return result;
}
