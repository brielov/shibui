#include "recentmodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QXmlStreamReader>
#include <QtConcurrentRun>

#include <algorithm>

RecentModel::RecentModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_watcher, &QFutureWatcher<LoadResult>::finished, this, [this] {
        const LoadResult result = m_watcher.result();
        beginResetModel();
        m_entries = result.entries;
        endResetModel();
        m_errorMessage = result.error;
        m_loading = false;
        emit countChanged();
        emit changed();
    });
}

RecentModel::~RecentModel()
{
    m_watcher.waitForFinished();
}

int RecentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant RecentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const RecentEntry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return entry.name;
    case PathRole:
        return entry.path;
    case RelativePathRole:
        return entry.parentPath;
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

QHash<int, QByteArray> RecentModel::roleNames() const
{
    return {
        {NameRole, "searchName"},
        {PathRole, "searchPath"},
        {RelativePathRole, "searchRelativePath"},
        {DirectoryRole, "searchIsDirectory"},
        {IconSourceRole, "searchIconSource"},
    };
}

bool RecentModel::loading() const { return m_loading; }
QString RecentModel::errorMessage() const { return m_errorMessage; }

void RecentModel::refresh()
{
    if (m_watcher.isRunning())
        return;
    m_loading = true;
    m_errorMessage.clear();
    emit changed();
    m_watcher.setFuture(QtConcurrent::run(&RecentModel::loadEntries));
}

QString RecentModel::pathAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).path : QString();
}

bool RecentModel::isDirectoryAt(int row) const
{
    return row >= 0 && row < m_entries.size() && m_entries.at(row).directory;
}

RecentModel::LoadResult RecentModel::loadEntries()
{
    LoadResult result;
    if (!qEnvironmentVariableIsSet("SHIBUI_SKIP_DESKTOP_QUERIES")) {
        const QString gsettings = QStandardPaths::findExecutable(QStringLiteral("gsettings"));
        if (!gsettings.isEmpty()) {
            QProcess process;
            process.start(gsettings,
                          {QStringLiteral("get"),
                           QStringLiteral("org.gnome.desktop.privacy"),
                           QStringLiteral("remember-recent-files")},
                          QIODevice::ReadOnly);
            if (process.waitForFinished(1500) && process.exitCode() == 0
                && QString::fromUtf8(process.readAllStandardOutput()).trimmed()
                       == QStringLiteral("false")) {
                result.error = QStringLiteral("Recent files are disabled in desktop privacy settings.");
                return result;
            }
        }
    }

    const QString path = QDir(QStandardPaths::writableLocation(
                                  QStandardPaths::GenericDataLocation))
                             .filePath(QStringLiteral("recently-used.xbel"));
    QFile file(path);
    if (!file.exists())
        return result;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = QStringLiteral("The desktop recent-file list could not be read.");
        return result;
    }

    QXmlStreamReader xml(&file);
    QSet<QString> seen;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QStringLiteral("bookmark"))
            continue;
        const QUrl url(xml.attributes().value(QStringLiteral("href")).toString());
        if (!url.isLocalFile())
            continue;
        const QString localPath = QDir::cleanPath(url.toLocalFile());
        const QFileInfo info(localPath);
        if ((!info.exists() && !info.isSymLink()) || seen.contains(localPath))
            continue;
        seen.insert(localPath);
        const QDateTime modified = QDateTime::fromString(
            xml.attributes().value(QStringLiteral("modified")).toString(), Qt::ISODate);
        result.entries.append({info.fileName().isEmpty() ? localPath : info.fileName(),
                               localPath, info.absolutePath(), info.isDir(), modified});
    }
    if (xml.hasError()) {
        result.entries.clear();
        result.error = QStringLiteral("The desktop recent-file list is malformed.");
        return result;
    }
    std::stable_sort(result.entries.begin(), result.entries.end(),
                     [](const RecentEntry &left, const RecentEntry &right) {
        return left.modified > right.modified;
    });
    if (result.entries.size() > 100)
        result.entries.resize(100);
    return result;
}
