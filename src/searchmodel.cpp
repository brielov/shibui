#include "searchmodel.h"

#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtConcurrentRun>

#include <algorithm>

namespace {
constexpr int maximumResults = 750;

int fuzzyScore(const QString &text, const QString &query)
{
    if (query.isEmpty())
        return 0;

    const QString candidate = text.toCaseFolded();
    const QString needle = query.toCaseFolded();
    int score = 0;
    int previous = -1;
    int first = -1;
    for (const QChar character : needle) {
        const int position = candidate.indexOf(character, previous + 1);
        if (position < 0)
            return -1;
        if (first < 0)
            first = position;
        score += 12;
        if (position == previous + 1)
            score += 18;
        if (position == 0 || QStringLiteral("/_-. ").contains(candidate.at(position - 1)))
            score += 14;
        if (previous >= 0)
            score -= qMin(12, position - previous - 1);
        previous = position;
    }
    score -= qMin(30, first);
    score -= candidate.size() / 8;
    if (candidate == needle)
        score += 220;
    else if (candidate.startsWith(needle))
        score += 110;
    else if (candidate.contains(needle))
        score += 65;
    return score;
}

bool matchesType(const SearchEntry &entry, const QString &typeFilter)
{
    if (typeFilter == QStringLiteral("all"))
        return true;
    if (typeFilter == QStringLiteral("folders"))
        return entry.directory;
    if (typeFilter == QStringLiteral("files"))
        return !entry.directory;
    if (entry.directory)
        return false;

    const QString extension = QFileInfo(entry.name).suffix().toCaseFolded();
    if (typeFilter == QStringLiteral("images")) {
        return QStringList{QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
                QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("svg"),
                QStringLiteral("bmp"), QStringLiteral("heic"), QStringLiteral("avif")}
            .contains(extension);
    }
    if (typeFilter == QStringLiteral("documents")) {
        return QStringList{QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("pdf"),
                QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("odt"),
                QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("ods"),
                QStringLiteral("ppt"), QStringLiteral("pptx"), QStringLiteral("odp"),
                QStringLiteral("csv")}.contains(extension);
    }
    if (typeFilter == QStringLiteral("audio")) {
        return QStringList{QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("wav"),
                QStringLiteral("ogg"), QStringLiteral("m4a"), QStringLiteral("opus")}
            .contains(extension);
    }
    if (typeFilter == QStringLiteral("video")) {
        return QStringList{QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("webm"),
                QStringLiteral("mov"), QStringLiteral("avi"), QStringLiteral("m4v")}
            .contains(extension);
    }
    if (typeFilter == QStringLiteral("archives")) {
        return QStringList{QStringLiteral("zip"), QStringLiteral("tar"), QStringLiteral("gz"),
                QStringLiteral("bz2"), QStringLiteral("xz"), QStringLiteral("zst"),
                QStringLiteral("7z"), QStringLiteral("rar")}.contains(extension);
    }
    return true;
}

SearchRankResult rankEntries(const QVector<QVector<SearchCandidate>> &batches,
                             const QString &basePath,
                             const QString &query, const QString &typeFilter,
                             int modifiedWithinDays, int generation)
{
    QVector<SearchEntry> matches;
    matches.reserve(maximumResults);
    const qint64 modifiedCutoff = modifiedWithinDays > 0
        ? QDateTime::currentDateTime().addDays(-modifiedWithinDays).toMSecsSinceEpoch() : 0;
    auto betterMatch = [](const SearchEntry &left, const SearchEntry &right) {
        if (left.score != right.score)
            return left.score > right.score;
        if (left.directory != right.directory)
            return left.directory;
        if (left.relativePath.size() != right.relativePath.size())
            return left.relativePath.size() < right.relativePath.size();
        return QString::compare(left.relativePath, right.relativePath,
                                Qt::CaseInsensitive) < 0;
    };

    for (const QVector<SearchCandidate> &batch : batches) {
        for (const SearchCandidate &candidate : batch) {
            SearchEntry entry{QFileInfo(candidate.relativePath).fileName(),
                              QDir(basePath).absoluteFilePath(candidate.relativePath),
                              candidate.relativePath, candidate.directory, 0};
            if (!matchesType(entry, typeFilter))
                continue;
            if (modifiedWithinDays > 0 && candidate.modifiedMilliseconds < modifiedCutoff)
                continue;
            if (query.isEmpty()) {
                entry.score = 0;
            } else {
                const int nameScore = fuzzyScore(entry.name, query);
                const int pathScore = fuzzyScore(entry.relativePath, query);
                if (nameScore < 0 && pathScore < 0)
                    continue;
                entry.score = qMax(nameScore < 0 ? nameScore : nameScore + 95, pathScore);
            }
            if (matches.size() < maximumResults) {
                matches.append(std::move(entry));
                std::push_heap(matches.begin(), matches.end(), betterMatch);
            } else if (betterMatch(entry, matches.constFirst())) {
                std::pop_heap(matches.begin(), matches.end(), betterMatch);
                matches.last() = std::move(entry);
                std::push_heap(matches.begin(), matches.end(), betterMatch);
            }
        }
    }
    std::sort(matches.begin(), matches.end(), betterMatch);
    return {generation, std::move(matches)};
}
}

SearchModel::SearchModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_rankTimer.setSingleShot(true);
    m_rankTimer.setInterval(150);
    connect(&m_rankTimer, &QTimer::timeout, this, &SearchModel::startRanking);
    connect(&m_rankWatcher, &QFutureWatcher<SearchRankResult>::finished, this, [this] {
        const SearchRankResult result = m_rankWatcher.result();
        if (result.generation == m_rankGeneration)
            replaceResults(result.entries);
        if (m_rankPending) {
            m_rankPending = false;
            m_rankTimer.start();
        }
    });
}

SearchModel::~SearchModel()
{
    cancel();
    m_rankWatcher.waitForFinished();
}

int SearchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_results.size();
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size())
        return {};
    const SearchEntry &entry = m_results.at(index.row());
    switch (role) {
    case NameRole:
        return entry.name;
    case PathRole:
        return entry.path;
    case RelativePathRole:
        return entry.relativePath;
    case DirectoryRole:
        return entry.directory;
    case IconSourceRole: {
        const QByteArray encoded = entry.path.toUtf8().toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        return QStringLiteral("image://fileicon/") + QString::fromLatin1(encoded);
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
    return {
        {NameRole, "searchName"},
        {PathRole, "searchPath"},
        {RelativePathRole, "searchRelativePath"},
        {DirectoryRole, "searchIsDirectory"},
        {IconSourceRole, "searchIconSource"},
    };
}

QString SearchModel::query() const
{
    return m_query;
}

void SearchModel::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    emit queryChanged();
    scheduleRanking();
}

QString SearchModel::basePath() const
{
    return m_basePath;
}

bool SearchModel::scanning() const
{
    return m_scanning;
}

QString SearchModel::errorMessage() const
{
    return m_errorMessage;
}

int SearchModel::scannedCount() const
{
    return m_scannedCount;
}

QString SearchModel::typeFilter() const
{
    return m_typeFilter;
}

void SearchModel::setTypeFilter(const QString &typeFilter)
{
    static const QStringList allowed = {
        QStringLiteral("all"), QStringLiteral("files"), QStringLiteral("folders"),
        QStringLiteral("images"), QStringLiteral("documents"), QStringLiteral("audio"),
        QStringLiteral("video"), QStringLiteral("archives"),
    };
    const QString normalized = typeFilter.toCaseFolded();
    if (!allowed.contains(normalized) || m_typeFilter == normalized)
        return;
    m_typeFilter = normalized;
    emit typeFilterChanged();
    scheduleRanking();
}

int SearchModel::modifiedWithinDays() const
{
    return m_modifiedWithinDays;
}

void SearchModel::setModifiedWithinDays(int days)
{
    days = qMax(0, days);
    if (m_modifiedWithinDays == days)
        return;
    m_modifiedWithinDays = days;
    emit modifiedWithinDaysChanged();
    scheduleRanking();
}

bool SearchModel::start(const QString &basePath, bool showHidden)
{
    cancel();
    const QFileInfo baseInfo(basePath);
    if (!baseInfo.isDir()) {
        setErrorMessage(QStringLiteral("The search location is not a directory."));
        return false;
    }
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("fd"));
    if (executable.isEmpty()) {
        setErrorMessage(QStringLiteral("Recursive finding requires the fd command."));
        return false;
    }

    m_basePath = QDir::cleanPath(baseInfo.absoluteFilePath());
    emit basePathChanged();
    setErrorMessage({});
    m_outputBuffer.clear();
    m_batches.clear();
    m_scannedCount = 0;
    emit scannedCountChanged();
    replaceResults({});

    const int generation = ++m_scanGeneration;
    auto *process = new QProcess(this);
    m_process = process;
    process->setWorkingDirectory(m_basePath);
    connect(process, &QProcess::readyReadStandardOutput, this,
            [this, process, generation] { consumeOutput(process, generation); });
    connect(process, &QProcess::finished, this,
            [this, process, generation](int exitCode, QProcess::ExitStatus exitStatus) {
                if (process != m_process || generation != m_scanGeneration) {
                    process->deleteLater();
                    return;
                }
                consumeOutput(process, generation, true);
                if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                    QString message = QString::fromUtf8(process->readAllStandardError()).trimmed();
                    if (message.isEmpty())
                        message = QStringLiteral("The recursive scan failed.");
                    setErrorMessage(message);
                }
                m_process = nullptr;
                process->deleteLater();
                setScanning(false);
                scheduleRanking();
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process, generation](QProcess::ProcessError processError) {
                if (processError != QProcess::FailedToStart
                    || process != m_process || generation != m_scanGeneration)
                    return;
                m_process = nullptr;
                setErrorMessage(process->errorString());
                process->deleteLater();
                setScanning(false);
            });

    QStringList arguments = {
        QStringLiteral("--color"), QStringLiteral("never"),
        QStringLiteral("--print0"),
        QStringLiteral("--no-require-git"),
        QStringLiteral("--no-ignore"),
        QStringLiteral("--type"), QStringLiteral("file"),
        QStringLiteral("--type"), QStringLiteral("directory"),
    };
    if (showHidden)
        arguments << QStringLiteral("--hidden");
    arguments << QStringLiteral(".");
    setScanning(true);
    process->start(executable, arguments, QIODevice::ReadOnly);
    return true;
}

void SearchModel::cancel()
{
    ++m_scanGeneration;
    ++m_rankGeneration;
    m_rankTimer.stop();
    m_rankPending = false;
    if (m_process) {
        QProcess *process = m_process;
        m_process = nullptr;
        process->disconnect(this);
        process->kill();
        process->deleteLater();
    }
    setScanning(false);
}

QString SearchModel::pathAt(int row) const
{
    return row >= 0 && row < m_results.size() ? m_results.at(row).path : QString();
}

bool SearchModel::isDirectoryAt(int row) const
{
    return row >= 0 && row < m_results.size() && m_results.at(row).directory;
}

void SearchModel::consumeOutput(QProcess *process, int generation, bool flush)
{
    if (process != m_process || generation != m_scanGeneration)
        return;
    m_outputBuffer += process->readAllStandardOutput();
    QVector<SearchCandidate> batch;
    qsizetype consumed = 0;
    qsizetype separator = -1;
    while ((separator = m_outputBuffer.indexOf('\0', consumed)) >= 0) {
        const QByteArray encoded = m_outputBuffer.mid(consumed, separator - consumed);
        consumed = separator + 1;
        if (encoded.isEmpty())
            continue;
        QString relative = QString::fromUtf8(encoded);
        bool directory = relative.endsWith(QLatin1Char('/'));
        while (relative.startsWith(QStringLiteral("./")))
            relative.remove(0, 2);
        while (relative.endsWith(QLatin1Char('/')))
            relative.chop(1);
        relative = QDir::cleanPath(relative);
        if (relative.isEmpty() || relative == QStringLiteral("."))
            continue;
        const QString path = QDir(m_basePath).absoluteFilePath(relative);
        batch.append({relative, directory,
                      QFileInfo(path).lastModified().toMSecsSinceEpoch()});
    }
    if (consumed > 0)
        m_outputBuffer.remove(0, consumed);
    if (flush && !m_outputBuffer.isEmpty()) {
        QString relative = QString::fromUtf8(m_outputBuffer);
        m_outputBuffer.clear();
        const bool directory = relative.endsWith(QLatin1Char('/'));
        while (relative.startsWith(QStringLiteral("./")))
            relative.remove(0, 2);
        while (relative.endsWith(QLatin1Char('/')))
            relative.chop(1);
        relative = QDir::cleanPath(relative);
        if (!relative.isEmpty() && relative != QStringLiteral(".")) {
            const QString path = QDir(m_basePath).absoluteFilePath(relative);
            batch.append({relative, directory,
                          QFileInfo(path).lastModified().toMSecsSinceEpoch()});
        }
    }
    if (batch.isEmpty())
        return;
    m_scannedCount += batch.size();
    m_batches.append(std::move(batch));
    emit scannedCountChanged();
    scheduleRanking();
}

void SearchModel::scheduleRanking()
{
    ++m_rankGeneration;
    if (m_rankWatcher.isRunning()) {
        m_rankPending = true;
        return;
    }
    m_rankTimer.start();
}

void SearchModel::startRanking()
{
    if (m_rankWatcher.isRunning()) {
        m_rankPending = true;
        return;
    }
    const QVector<QVector<SearchCandidate>> batches = m_batches;
    const QString basePath = m_basePath;
    const QString query = m_query;
    const QString typeFilter = m_typeFilter;
    const int modifiedWithinDays = m_modifiedWithinDays;
    const int generation = m_rankGeneration;
    m_rankWatcher.setFuture(QtConcurrent::run(
        [batches, basePath, query, typeFilter, modifiedWithinDays, generation] {
        return rankEntries(batches, basePath, query, typeFilter, modifiedWithinDays, generation);
    }));
}

void SearchModel::replaceResults(QVector<SearchEntry> entries)
{
    beginResetModel();
    m_results = std::move(entries);
    endResetModel();
    emit countChanged();
}

void SearchModel::setScanning(bool scanning)
{
    if (m_scanning == scanning)
        return;
    m_scanning = scanning;
    emit scanningChanged();
}

void SearchModel::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message)
        return;
    m_errorMessage = message;
    emit errorMessageChanged();
}
