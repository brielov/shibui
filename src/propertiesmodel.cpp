#include "propertiesmodel.h"

#include <QDateTime>
#include <QDirIterator>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QStorageInfo>
#include <QtConcurrentRun>

#include <unistd.h>

namespace {
QString formattedSize(quint64 bytes)
{
    return QLocale().formattedDataSize(static_cast<qint64>(bytes), 1,
                                       QLocale::DataSizeTraditionalFormat);
}

QString permissionText(QFile::Permissions permissions)
{
    QString text;
    const struct { QFile::Permission permission; QChar character; } bits[] = {
        {QFile::ReadOwner, QLatin1Char('r')}, {QFile::WriteOwner, QLatin1Char('w')},
        {QFile::ExeOwner, QLatin1Char('x')}, {QFile::ReadGroup, QLatin1Char('r')},
        {QFile::WriteGroup, QLatin1Char('w')}, {QFile::ExeGroup, QLatin1Char('x')},
        {QFile::ReadOther, QLatin1Char('r')}, {QFile::WriteOther, QLatin1Char('w')},
        {QFile::ExeOther, QLatin1Char('x')},
    };
    for (const auto &bit : bits)
        text += permissions.testFlag(bit.permission) ? bit.character : QLatin1Char('-');
    return text;
}

int permissionModeValue(QFile::Permissions permissions)
{
    const QFile::Permission bits[] = {
        QFile::ReadOwner, QFile::WriteOwner, QFile::ExeOwner,
        QFile::ReadGroup, QFile::WriteGroup, QFile::ExeGroup,
        QFile::ReadOther, QFile::WriteOther, QFile::ExeOther,
    };
    int mode = 0;
    for (int index = 0; index < 9; ++index) {
        if (permissions.testFlag(bits[index]))
            mode |= 1 << (8 - index);
    }
    return mode;
}

QString formattedDate(const QDateTime &date)
{
    return date.isValid() ? QLocale().toString(date, QLocale::ShortFormat)
                          : QStringLiteral("—");
}

FileProperties readProperties(const QString &path, int generation)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return {generation, {}, path, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, false,
                false, 0,
                QStringLiteral("The item no longer exists.")};
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info);
    const QStorageInfo storage(info.absoluteFilePath());
    const bool directory = info.isDir();
    return {
        generation,
        info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName(),
        info.absoluteFilePath(),
        mime.name(),
        directory ? QStringLiteral("Folder") : mime.comment(),
        directory ? QStringLiteral("Calculate on request")
                  : formattedSize(static_cast<quint64>(info.size())),
        formattedDate(info.lastModified()),
        formattedDate(info.birthTime()),
        formattedDate(info.lastRead()),
        info.owner().isEmpty() ? QStringLiteral("—") : info.owner(),
        info.group().isEmpty() ? QStringLiteral("—") : info.group(),
        permissionText(info.permissions()),
        info.isSymLink() ? info.symLinkTarget() : QStringLiteral("—"),
        storage.isValid() && storage.isReady()
            ? formattedSize(static_cast<quint64>(storage.bytesAvailable())) : QStringLiteral("—"),
        directory,
        info.ownerId() == static_cast<uint>(geteuid()),
        permissionModeValue(info.permissions()),
        {},
    };
}
}

PropertiesModel::PropertiesModel(QObject *parent)
    : QObject(parent)
{
    connect(&m_propertiesWatcher, &QFutureWatcher<FileProperties>::finished, this, [this] {
        const FileProperties properties = m_propertiesWatcher.result();
        if (!m_active || properties.generation != m_generation)
            return;
        m_properties = properties;
        m_loading = false;
        emit changed();
    });
    connect(&m_sizeWatcher, &QFutureWatcher<DirectorySizeResult>::finished, this, [this] {
        const DirectorySizeResult result = m_sizeWatcher.result();
        if (!m_active || result.generation != m_generation)
            return;
        m_sizing = false;
        if (result.cancelled) {
            m_recursiveSize = QStringLiteral("Cancelled");
        } else if (!result.error.isEmpty()) {
            m_recursiveSize = result.error;
        } else {
            m_recursiveSize = QStringLiteral("%1 in %2 item%3")
                .arg(formattedSize(result.bytes))
                .arg(result.items)
                .arg(result.items == 1 ? QString() : QStringLiteral("s"));
        }
        m_sizeCancelled.reset();
        emit changed();
    });
}

PropertiesModel::~PropertiesModel()
{
    cancelWork();
    m_propertiesWatcher.waitForFinished();
    m_sizeWatcher.waitForFinished();
}

bool PropertiesModel::active() const { return m_active; }
bool PropertiesModel::loading() const { return m_loading; }
bool PropertiesModel::sizing() const { return m_sizing; }
bool PropertiesModel::directory() const { return m_properties.directory; }
QString PropertiesModel::name() const { return m_properties.name; }
QString PropertiesModel::path() const { return m_properties.path; }
QString PropertiesModel::mimeType() const { return m_properties.mimeType; }
QString PropertiesModel::type() const { return m_properties.type; }
QString PropertiesModel::size() const { return m_properties.size; }
QString PropertiesModel::modified() const { return m_properties.modified; }
QString PropertiesModel::created() const { return m_properties.created; }
QString PropertiesModel::accessed() const { return m_properties.accessed; }
QString PropertiesModel::owner() const { return m_properties.owner; }
QString PropertiesModel::group() const { return m_properties.group; }
QString PropertiesModel::permissions() const { return m_properties.permissions; }
QString PropertiesModel::symlinkTarget() const { return m_properties.symlinkTarget; }
QString PropertiesModel::filesystemFree() const { return m_properties.filesystemFree; }
QString PropertiesModel::recursiveSize() const { return m_recursiveSize; }
QString PropertiesModel::errorMessage() const { return m_properties.error; }
bool PropertiesModel::permissionsEditable() const { return m_properties.permissionsEditable; }
int PropertiesModel::permissionMode() const { return m_properties.permissionMode; }

bool PropertiesModel::open(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return false;
    cancelWork();
    const int generation = ++m_generation;
    m_sizing = false;
    m_sizeCancelled.reset();
    m_properties = {};
    m_properties.path = info.absoluteFilePath();
    m_properties.name = info.fileName();
    m_recursiveSize.clear();
    m_active = true;
    m_loading = true;
    emit activeChanged();
    emit changed();
    m_propertiesWatcher.setFuture(QtConcurrent::run([path, generation] {
        return readProperties(path, generation);
    }));
    return true;
}

void PropertiesModel::close()
{
    if (!m_active)
        return;
    cancelWork();
    m_active = false;
    m_loading = false;
    m_sizing = false;
    emit activeChanged();
    emit changed();
}

bool PropertiesModel::calculateDirectorySize()
{
    if (!m_active || m_loading || !m_properties.directory || m_sizing)
        return false;
    const QString path = m_properties.path;
    const int generation = m_generation;
    m_sizeCancelled = std::make_shared<std::atomic_bool>(false);
    const auto cancelled = m_sizeCancelled;
    m_recursiveSize = QStringLiteral("Calculating…");
    m_sizing = true;
    emit changed();
    m_sizeWatcher.setFuture(QtConcurrent::run([path, generation, cancelled] {
        quint64 bytes = 0;
        quint64 items = 0;
        QDirIterator iterator(path,
                              QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot
                                  | QDir::System,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            if (cancelled->load(std::memory_order_relaxed))
                return DirectorySizeResult{generation, bytes, items, true, {}};
            iterator.next();
            const QFileInfo info = iterator.fileInfo();
            if (!info.isDir() || info.isSymLink())
                bytes += static_cast<quint64>(qMax<qint64>(0, info.size()));
            ++items;
        }
        return DirectorySizeResult{generation, bytes, items, false, {}};
    }));
    return true;
}

void PropertiesModel::cancelDirectorySize()
{
    if (m_sizeCancelled)
        m_sizeCancelled->store(true, std::memory_order_relaxed);
}

bool PropertiesModel::togglePermissionBit(int index)
{
    if (!m_active || m_loading || !m_properties.permissionsEditable
        || index < 0 || index >= 9)
        return false;
    const QFile::Permission bits[] = {
        QFile::ReadOwner, QFile::WriteOwner, QFile::ExeOwner,
        QFile::ReadGroup, QFile::WriteGroup, QFile::ExeGroup,
        QFile::ReadOther, QFile::WriteOther, QFile::ExeOther,
    };
    QFile::Permissions permissions = QFileInfo(m_properties.path).permissions();
    permissions ^= bits[index];
    if (!QFile::setPermissions(m_properties.path, permissions)) {
        m_properties.error = QStringLiteral("Could not change permissions.");
        emit changed();
        return false;
    }
    m_properties.error.clear();
    m_properties.permissions = permissionText(permissions);
    m_properties.permissionMode = permissionModeValue(permissions);
    emit changed();
    return true;
}

void PropertiesModel::cancelWork()
{
    ++m_generation;
    cancelDirectorySize();
}
