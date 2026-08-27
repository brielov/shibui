#include "bulkrenamemodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUuid>
#include <QtConcurrentRun>

#include <algorithm>

BulkRenameModel::BulkRenameModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_watcher, &QFutureWatcher<BulkRenameResult>::finished, this, [this] {
        const BulkRenameResult result = m_watcher.result();
        m_applying = false;
        m_errorMessage = result.success ? QString() : result.message;
        if (result.success)
            m_active = false;
        emit changed();
        emit finished(result.success, result.message, result.targetPaths);
    });
}

BulkRenameModel::~BulkRenameModel()
{
    m_watcher.waitForFinished();
}

int BulkRenameModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant BulkRenameModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const BulkRenameEntry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case OriginalNameRole:
        return entry.originalName;
    case ProposedNameRole:
        return entry.proposedName;
    case ValidRole:
        return entry.error.isEmpty();
    case ErrorRole:
        return entry.error;
    default:
        return {};
    }
}

QHash<int, QByteArray> BulkRenameModel::roleNames() const
{
    return {
        {OriginalNameRole, "originalName"},
        {ProposedNameRole, "proposedName"},
        {ValidRole, "renameValid"},
        {ErrorRole, "renameError"},
    };
}

bool BulkRenameModel::active() const { return m_active; }
bool BulkRenameModel::applying() const { return m_applying; }
QString BulkRenameModel::findText() const { return m_findText; }
QString BulkRenameModel::replacementText() const { return m_replacementText; }
bool BulkRenameModel::numbering() const { return m_numbering; }
bool BulkRenameModel::canApply() const { return m_canApply && !m_applying; }
QString BulkRenameModel::errorMessage() const { return m_errorMessage; }

void BulkRenameModel::setFindText(const QString &text)
{
    if (m_findText == text)
        return;
    m_findText = text;
    rebuildPreview();
}

void BulkRenameModel::setReplacementText(const QString &text)
{
    if (m_replacementText == text)
        return;
    m_replacementText = text;
    rebuildPreview();
}

void BulkRenameModel::setNumbering(bool numbering)
{
    if (m_numbering == numbering)
        return;
    m_numbering = numbering;
    rebuildPreview();
}

bool BulkRenameModel::begin(const QStringList &paths)
{
    if (m_applying || paths.size() < 2)
        return false;
    QVector<BulkRenameEntry> entries;
    entries.reserve(paths.size());
    QString parentPath;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.exists() && !info.isSymLink())
            return false;
        if (parentPath.isEmpty())
            parentPath = info.absolutePath();
        else if (parentPath != info.absolutePath())
            return false;
        entries.append({info.absoluteFilePath(), info.fileName(), info.fileName(), {}});
    }
    std::sort(entries.begin(), entries.end(), [](const BulkRenameEntry &left,
                                                  const BulkRenameEntry &right) {
        return QString::localeAwareCompare(left.originalName, right.originalName) < 0;
    });
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    m_findText.clear();
    m_replacementText.clear();
    m_errorMessage.clear();
    m_numbering = false;
    m_active = true;
    rebuildPreview();
    emit changed();
    return true;
}

void BulkRenameModel::close()
{
    if (m_applying)
        return;
    m_active = false;
    m_errorMessage.clear();
    emit changed();
}

bool BulkRenameModel::apply()
{
    if (!m_active || !canApply())
        return false;
    m_applying = true;
    m_errorMessage.clear();
    const QVector<BulkRenameEntry> entries = m_entries;
    m_watcher.setFuture(QtConcurrent::run([entries] { return renameAll(entries); }));
    emit changed();
    return true;
}

void BulkRenameModel::rebuildPreview()
{
    if (!m_active)
        return;
    QSet<QString> sourcePaths;
    for (const BulkRenameEntry &entry : std::as_const(m_entries))
        sourcePaths.insert(entry.sourcePath);

    const int width = std::max(2, int(QString::number(m_entries.size()).size()));
    QSet<QString> targets;
    bool changed = false;
    bool valid = true;
    for (int index = 0; index < m_entries.size(); ++index) {
        BulkRenameEntry &entry = m_entries[index];
        if (m_numbering) {
            const QFileInfo sourceInfo(entry.sourcePath);
            const QString suffix = sourceInfo.isDir() || sourceInfo.completeSuffix().isEmpty()
                ? QString() : QLatin1Char('.') + sourceInfo.completeSuffix();
            const QString base = m_replacementText.trimmed().isEmpty()
                ? QStringLiteral("Item") : m_replacementText.trimmed();
            entry.proposedName = QStringLiteral("%1 %2%3")
                .arg(base).arg(index + 1, width, 10, QLatin1Char('0')).arg(suffix);
        } else {
            entry.proposedName = entry.originalName;
            if (!m_findText.isEmpty())
                entry.proposedName.replace(m_findText, m_replacementText,
                                           Qt::CaseSensitive);
        }
        entry.error.clear();
        if (entry.proposedName.isEmpty() || entry.proposedName == QStringLiteral(".")
            || entry.proposedName == QStringLiteral("..")
            || entry.proposedName.contains(QLatin1Char('/'))
            || entry.proposedName.contains(QLatin1Char('\\'))) {
            entry.error = QStringLiteral("Invalid name");
        }
        const QString targetPath = QDir(QFileInfo(entry.sourcePath).absolutePath())
                                       .filePath(entry.proposedName);
        if (entry.error.isEmpty() && targets.contains(targetPath))
            entry.error = QStringLiteral("Duplicate target");
        if (entry.error.isEmpty() && QFileInfo::exists(targetPath)
            && !sourcePaths.contains(targetPath))
            entry.error = QStringLiteral("Already exists");
        targets.insert(targetPath);
        valid &= entry.error.isEmpty();
        changed |= entry.proposedName != entry.originalName;
    }
    m_canApply = valid && changed && (m_numbering || !m_findText.isEmpty());
    if (!m_entries.isEmpty())
        emit dataChanged(index(0), index(m_entries.size() - 1));
    emit previewChanged();
}

BulkRenameResult BulkRenameModel::renameAll(const QVector<BulkRenameEntry> &entries)
{
    struct RenameStep {
        QString source;
        QString temporary;
        QString target;
        bool staged = false;
        bool committed = false;
    };
    QVector<RenameStep> steps;
    steps.reserve(entries.size());
    const QString operationId = QUuid::createUuid().toString(QUuid::Id128);
    for (int index = 0; index < entries.size(); ++index) {
        const BulkRenameEntry &entry = entries.at(index);
        if (entry.originalName == entry.proposedName)
            continue;
        const QString parent = QFileInfo(entry.sourcePath).absolutePath();
        steps.append({entry.sourcePath,
                      QDir(parent).filePath(QStringLiteral(".shibui-rename-%1-%2")
                                               .arg(operationId).arg(index)),
                      QDir(parent).filePath(entry.proposedName)});
    }

    auto failure = [&steps](const QString &message) {
        for (int index = steps.size() - 1; index >= 0; --index) {
            RenameStep &step = steps[index];
            if (step.committed) {
                QFile::rename(step.target, step.temporary);
                step.committed = false;
                step.staged = true;
            }
        }
        for (int index = steps.size() - 1; index >= 0; --index) {
            RenameStep &step = steps[index];
            if (step.staged)
                QFile::rename(step.temporary, step.source);
        }
        return BulkRenameResult{false, message, {}};
    };
    for (RenameStep &step : steps) {
        if (QFileInfo::exists(step.temporary) || !QFile::rename(step.source, step.temporary))
            return failure(QStringLiteral("Could not prepare “%1” for renaming.")
                               .arg(QFileInfo(step.source).fileName()));
        step.staged = true;
    }
    QStringList targets;
    for (RenameStep &step : steps) {
        if (!QFile::rename(step.temporary, step.target))
            return failure(QStringLiteral("Could not rename “%1” to “%2”.")
                               .arg(QFileInfo(step.source).fileName(),
                                    QFileInfo(step.target).fileName()));
        step.committed = true;
        targets << step.target;
    }
    return {true,
            QStringLiteral("Renamed %1 item%2.")
                .arg(targets.size()).arg(targets.size() == 1 ? QString() : QStringLiteral("s")),
            targets};
}
