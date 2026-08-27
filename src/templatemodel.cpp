#include "templatemodel.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtConcurrentRun>

#include <algorithm>

TemplateModel::TemplateModel(const QString &templatesPath, QObject *parent)
    : QAbstractListModel(parent)
    , m_templatesPath(templatesPath.isEmpty()
          ? QStandardPaths::writableLocation(QStandardPaths::TemplatesLocation)
          : QDir::cleanPath(templatesPath))
{
    connect(&m_loadWatcher, &QFutureWatcher<QVector<TemplateEntry>>::finished,
            this, [this] {
        beginResetModel();
        m_entries = m_loadWatcher.result();
        endResetModel();
        m_loading = false;
        if (m_entries.isEmpty())
            m_errorMessage = QStringLiteral("The Templates folder contains no files.");
        emit countChanged();
        emit changed();
    });
    connect(&m_copyWatcher, &QFutureWatcher<TemplateCopyResult>::finished,
            this, [this] {
        const TemplateCopyResult result = m_copyWatcher.result();
        m_copying = false;
        m_errorMessage = result.success ? QString() : result.message;
        if (result.success)
            m_active = false;
        emit changed();
        emit finished(result.success, result.message, result.outputPath);
    });
}

TemplateModel::~TemplateModel()
{
    m_loadWatcher.waitForFinished();
    m_copyWatcher.waitForFinished();
}

int TemplateModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant TemplateModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const TemplateEntry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return entry.name;
    case PathRole:
        return entry.path;
    case RelativePathRole:
        return entry.relativePath;
    default:
        return {};
    }
}

QHash<int, QByteArray> TemplateModel::roleNames() const
{
    return {
        {NameRole, "templateName"},
        {PathRole, "templatePath"},
        {RelativePathRole, "templateRelativePath"},
    };
}

bool TemplateModel::active() const { return m_active; }
bool TemplateModel::loading() const { return m_loading; }
bool TemplateModel::copying() const { return m_copying; }
QString TemplateModel::errorMessage() const { return m_errorMessage; }

bool TemplateModel::begin(const QString &destinationDirectory)
{
    if (m_loading || m_copying || !QFileInfo(destinationDirectory).isDir())
        return false;
    m_destinationDirectory = QDir::cleanPath(destinationDirectory);
    m_errorMessage.clear();
    m_active = true;
    m_loading = true;
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
    emit changed();
    const QString templatesPath = m_templatesPath;
    m_loadWatcher.setFuture(QtConcurrent::run(
        [templatesPath] { return loadTemplates(templatesPath); }));
    return true;
}

void TemplateModel::close()
{
    if (m_copying)
        return;
    m_active = false;
    m_errorMessage.clear();
    emit changed();
}

QString TemplateModel::suggestedNameAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).name : QString();
}

bool TemplateModel::createFrom(int row, const QString &name)
{
    if (!m_active || m_copying || row < 0 || row >= m_entries.size())
        return false;
    const QString outputName = name.trimmed();
    if (outputName.isEmpty() || outputName == QStringLiteral(".")
        || outputName == QStringLiteral("..") || outputName.contains(QLatin1Char('/'))
        || outputName.contains(QLatin1Char('\\'))) {
        m_errorMessage = QStringLiteral("Use a plain file name without path separators.");
        emit changed();
        return false;
    }
    const QString outputPath = QDir(m_destinationDirectory).filePath(outputName);
    if (QFileInfo::exists(outputPath)) {
        m_errorMessage = QStringLiteral("“%1” already exists.").arg(outputName);
        emit changed();
        return false;
    }
    const QString sourcePath = m_entries.at(row).path;
    m_copying = true;
    m_errorMessage.clear();
    emit changed();
    m_copyWatcher.setFuture(QtConcurrent::run(
        [sourcePath, outputPath] { return copyTemplate(sourcePath, outputPath); }));
    return true;
}

QVector<TemplateEntry> TemplateModel::loadTemplates(const QString &rootPath)
{
    QVector<TemplateEntry> entries;
    const QDir root(rootPath);
    if (!root.exists())
        return entries;
    QDirIterator iterator(rootPath, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info(path);
        entries.append({info.fileName(), info.absoluteFilePath(), root.relativeFilePath(path)});
    }
    std::sort(entries.begin(), entries.end(), [](const TemplateEntry &left,
                                                  const TemplateEntry &right) {
        return QString::localeAwareCompare(left.relativePath, right.relativePath) < 0;
    });
    return entries;
}

TemplateCopyResult TemplateModel::copyTemplate(const QString &sourcePath,
                                                const QString &outputPath)
{
    QFile source(sourcePath);
    if (!source.copy(outputPath)) {
        QFile::remove(outputPath);
        return {false,
                QStringLiteral("Could not create “%1”: %2")
                    .arg(QFileInfo(outputPath).fileName(), source.errorString()), {}};
    }
    QFile output(outputPath);
    output.setPermissions(output.permissions() | QFileDevice::WriteOwner);
    return {true, QStringLiteral("Created %1").arg(outputPath), outputPath};
}
