#include "archivemodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace {
const QStringList supportedArchiveSuffixes = {
    QStringLiteral(".zip"), QStringLiteral(".tar"), QStringLiteral(".tar.gz"),
    QStringLiteral(".tgz"), QStringLiteral(".tar.bz2"), QStringLiteral(".tbz2"),
    QStringLiteral(".tar.xz"), QStringLiteral(".txz"), QStringLiteral(".7z"),
    QStringLiteral(".rar"),
};

const QStringList creatableArchiveSuffixes = {
    QStringLiteral(".zip"), QStringLiteral(".tar"), QStringLiteral(".tar.gz"),
    QStringLiteral(".tgz"), QStringLiteral(".tar.bz2"), QStringLiteral(".tbz2"),
    QStringLiteral(".tar.xz"), QStringLiteral(".txz"),
};

bool endsWithAny(const QString &name, const QStringList &suffixes)
{
    const QString folded = name.toCaseFolded();
    for (const QString &suffix : suffixes) {
        if (folded.endsWith(suffix))
            return true;
    }
    return false;
}

bool pathExists(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() || info.isSymLink();
}

bool publishArchive(const QString &workPath, const QString &outputPath, QString &error)
{
    if (::link(QFile::encodeName(workPath).constData(),
               QFile::encodeName(outputPath).constData()) != 0) {
        error = errno == EEXIST
            ? QStringLiteral("The archive destination was created by another operation.")
            : QStringLiteral("Could not publish the archive: %1")
                  .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    if (!QFile::remove(workPath)) {
        error = QStringLiteral("The archive was created, but its temporary link remains at %1.")
                    .arg(workPath);
    }
    return true;
}
}

ArchiveModel::ArchiveModel(QObject *parent)
    : QObject(parent)
{
}

ArchiveModel::~ArchiveModel()
{
    cancel();
}

bool ArchiveModel::active() const { return m_process != nullptr; }
QString ArchiveModel::description() const { return m_description; }
QString ArchiveModel::errorMessage() const { return m_errorMessage; }

bool ArchiveModel::supportsArchive(const QString &path) const
{
    return QFileInfo(path).isFile()
        && endsWithAny(QFileInfo(path).fileName(), supportedArchiveSuffixes);
}

bool ArchiveModel::createArchive(const QStringList &paths,
                                 const QString &destinationDirectory,
                                 const QString &name)
{
    if (active()) {
        fail(QStringLiteral("Another archive operation is already active."));
        return false;
    }
    if (paths.isEmpty()) {
        fail(QStringLiteral("Choose at least one item to archive."));
        return false;
    }
    const QFileInfo destinationInfo(destinationDirectory);
    if (!destinationInfo.isDir()) {
        fail(QStringLiteral("The archive destination is unavailable."));
        return false;
    }

    QString archiveName = name.trimmed();
    if (!endsWithAny(archiveName, creatableArchiveSuffixes))
        archiveName += QStringLiteral(".zip");
    if (archiveName.isEmpty() || archiveName == QStringLiteral(".")
        || archiveName == QStringLiteral("..") || archiveName.contains(QLatin1Char('/'))
        || archiveName.contains(QLatin1Char('\\'))) {
        fail(QStringLiteral("Use a plain archive name without path separators."));
        return false;
    }

    const QString workingDirectory = QFileInfo(paths.constFirst()).absolutePath();
    QStringList sourceNames;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if ((!info.exists() && !info.isSymLink()) || info.absolutePath() != workingDirectory) {
            fail(QStringLiteral("Archive items must exist in the same folder."));
            return false;
        }
        sourceNames << info.fileName();
    }
    const QString outputPath = QDir(destinationDirectory).filePath(archiveName);
    if (pathExists(outputPath)) {
        fail(QStringLiteral("“%1” already exists.").arg(archiveName));
        return false;
    }

    QTemporaryFile temporary(
        QDir(destinationDirectory).filePath(QStringLiteral(".shibui-archive-XXXXXX")));
    temporary.setAutoRemove(false);
    if (!temporary.open()) {
        fail(QStringLiteral("Could not reserve temporary archive storage."));
        return false;
    }
    const QString workPath = temporary.fileName();
    temporary.close();

    QStringList arguments = {QStringLiteral("-a"), QStringLiteral("-cf"), workPath,
                             QStringLiteral("--")};
    arguments += sourceNames;
    m_description = QStringLiteral("Creating %1").arg(archiveName);
    if (!start(arguments, workingDirectory, outputPath, workPath, false)) {
        QFile::remove(workPath);
        return false;
    }
    return true;
}

bool ArchiveModel::extractArchive(const QString &path,
                                  const QString &destinationDirectory)
{
    if (active()) {
        fail(QStringLiteral("Another archive operation is already active."));
        return false;
    }
    if (!supportsArchive(path)) {
        fail(QStringLiteral("Choose a supported ZIP, tar, 7z, or RAR archive."));
        return false;
    }
    if (!QFileInfo(destinationDirectory).isDir()) {
        fail(QStringLiteral("The extraction destination is unavailable."));
        return false;
    }
    if (QStandardPaths::findExecutable(QStringLiteral("bsdtar")).isEmpty()) {
        fail(QStringLiteral("bsdtar is required for archive operations."));
        return false;
    }

    const QString base = archiveBaseName(QFileInfo(path).fileName());
    QString folderName = base.isEmpty() ? QStringLiteral("Archive") : base;
    QString outputPath;
    for (int suffix = 1; ; ++suffix) {
        const QString candidateName = suffix == 1
            ? folderName : QStringLiteral("%1 (%2)").arg(folderName).arg(suffix);
        const QString candidate = QDir(destinationDirectory).filePath(candidateName);
        if (QDir(destinationDirectory).mkdir(candidateName)) {
            outputPath = candidate;
            break;
        }
        if (!pathExists(candidate)) {
            fail(QStringLiteral("Could not create the extraction folder."));
            return false;
        }
    }

    m_description = QStringLiteral("Extracting %1").arg(QFileInfo(path).fileName());
    return start({QStringLiteral("-xf"), QFileInfo(path).absoluteFilePath(),
                  QStringLiteral("-C"), outputPath}, destinationDirectory,
                 outputPath, outputPath, true);
}

void ArchiveModel::cancel()
{
    if (!m_process)
        return;
    m_process->kill();
}

bool ArchiveModel::start(const QStringList &arguments, const QString &workingDirectory,
                         const QString &outputPath, const QString &workPath, bool extracting)
{
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("bsdtar"));
    if (executable.isEmpty()) {
        fail(QStringLiteral("bsdtar is required for archive operations."));
        return false;
    }

    m_errorMessage.clear();
    m_outputPath = outputPath;
    m_workPath = workPath;
    m_extracting = extracting;
    auto *process = new QProcess(this);
    m_process = process;
    process->setWorkingDirectory(workingDirectory);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                finishProcess(process,
                              exitStatus == QProcess::NormalExit && exitCode == 0,
                              exitStatus == QProcess::CrashExit,
                              QString::fromUtf8(process->readAll()).trimmed());
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError processError) {
                if (processError == QProcess::FailedToStart)
                    finishProcess(process, false, false, process->errorString());
            });
    process->start(executable, arguments, QIODevice::ReadOnly);
    emit changed();
    return true;
}

void ArchiveModel::finishProcess(QProcess *process, bool success, bool cancelled,
                                 const QString &detail)
{
    if (process != m_process) {
        process->deleteLater();
        return;
    }

    const QString outputPath = m_outputPath;
    const QString workPath = m_workPath;
    const bool extracting = m_extracting;
    QString finalDetail = detail;
    if (success && !extracting)
        success = publishArchive(workPath, outputPath, finalDetail);
    if (!success && !extracting)
        QFile::remove(workPath);

    QString message;
    if (success) {
        message = extracting
            ? QStringLiteral("Extracted to %1").arg(outputPath)
            : QStringLiteral("Created %1").arg(outputPath);
    } else if (cancelled) {
        message = extracting
            ? QStringLiteral("Extraction cancelled; partial files may remain in %1.").arg(outputPath)
            : QStringLiteral("Archive creation cancelled.");
    } else {
        message = finalDetail.isEmpty() ? QStringLiteral("The archive operation failed.") : finalDetail;
        if (extracting)
            message += QStringLiteral(" Partial files may remain in %1.").arg(outputPath);
    }

    m_process = nullptr;
    m_description.clear();
    m_errorMessage = success ? QString() : message;
    process->deleteLater();
    emit changed();
    emit finished(success, message, outputPath);
}

void ArchiveModel::fail(const QString &message)
{
    m_errorMessage = message;
    emit changed();
}

QString ArchiveModel::archiveBaseName(const QString &fileName)
{
    const QString folded = fileName.toCaseFolded();
    for (const QString &suffix : supportedArchiveSuffixes) {
        if (folded.endsWith(suffix))
            return fileName.left(fileName.size() - suffix.size());
    }
    return QFileInfo(fileName).completeBaseName();
}
