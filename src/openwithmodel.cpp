#include "openwithmodel.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

namespace {
QString applicationName(const QString &desktopId)
{
    const QString path = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation, QStringLiteral("applications/") + desktopId);
    if (path.isEmpty())
        return desktopId.chopped(QStringLiteral(".desktop").size());
    QSettings desktop(path, QSettings::IniFormat);
    desktop.beginGroup(QStringLiteral("Desktop Entry"));
    const QString name = desktop.value(QStringLiteral("Name")).toString();
    desktop.endGroup();
    return name.isEmpty() ? desktopId.chopped(QStringLiteral(".desktop").size()) : name;
}
}

OpenWithModel::OpenWithModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

OpenWithModel::~OpenWithModel()
{
    stopProcess();
}

int OpenWithModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_applications.size();
}

QVariant OpenWithModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_applications.size())
        return {};
    const ApplicationEntry &application = m_applications.at(index.row());
    switch (role) {
    case NameRole:
        return application.name;
    case DesktopIdRole:
        return application.desktopId;
    case DefaultRole:
        return application.isDefault;
    default:
        return {};
    }
}

QHash<int, QByteArray> OpenWithModel::roleNames() const
{
    return {
        {NameRole, "applicationName"},
        {DesktopIdRole, "applicationDesktopId"},
        {DefaultRole, "applicationIsDefault"},
    };
}

bool OpenWithModel::active() const { return m_active; }
bool OpenWithModel::loading() const { return m_loading; }
QString OpenWithModel::path() const { return m_path; }
QString OpenWithModel::mimeType() const { return m_mimeType; }
QString OpenWithModel::errorMessage() const { return m_errorMessage; }

bool OpenWithModel::open(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isFile())
        return false;
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("gio"));
    if (executable.isEmpty()) {
        setErrorMessage(QStringLiteral("The gio command is unavailable."));
        return false;
    }

    stopProcess();
    m_path = info.absoluteFilePath();
    m_mimeType = QMimeDatabase().mimeTypeForFile(info).name();
    m_active = true;
    emit activeChanged();
    emit pathChanged();
    setErrorMessage({});
    replaceApplications({});
    setLoading(true);

    auto *process = new QProcess(this);
    m_process = process;
    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                if (process != m_process) {
                    process->deleteLater();
                    return;
                }
                m_process = nullptr;
                QVector<ApplicationEntry> applications;
                if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    const QString output = QString::fromUtf8(process->readAllStandardOutput());
                    const QRegularExpression desktopPattern(
                        QStringLiteral("([A-Za-z0-9_.+-]+\\.desktop)"));
                    QString defaultId;
                    const QString firstLine = output.section(QLatin1Char('\n'), 0, 0);
                    auto defaultMatch = desktopPattern.match(firstLine);
                    if (defaultMatch.hasMatch())
                        defaultId = defaultMatch.captured(1);
                    QSet<QString> seen;
                    auto iterator = desktopPattern.globalMatch(output);
                    while (iterator.hasNext()) {
                        const QString desktopId = iterator.next().captured(1);
                        if (seen.contains(desktopId))
                            continue;
                        seen.insert(desktopId);
                        applications.append({applicationName(desktopId), desktopId,
                                             desktopId == defaultId});
                    }
                } else {
                    setErrorMessage(QString::fromUtf8(
                        process->readAllStandardError()).trimmed());
                }
                process->deleteLater();
                replaceApplications(std::move(applications));
                if (m_applications.isEmpty() && m_errorMessage.isEmpty())
                    setErrorMessage(QStringLiteral("No compatible applications were found."));
                setLoading(false);
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError processError) {
                if (processError != QProcess::FailedToStart || process != m_process)
                    return;
                m_process = nullptr;
                setErrorMessage(process->errorString());
                process->deleteLater();
                replaceApplications({});
                setLoading(false);
            });
    process->start(executable, {QStringLiteral("mime"), m_mimeType}, QIODevice::ReadOnly);
    return true;
}

void OpenWithModel::close()
{
    stopProcess();
    if (!m_active)
        return;
    m_active = false;
    setLoading(false);
    emit activeChanged();
}

QString OpenWithModel::desktopIdAt(int row) const
{
    return row >= 0 && row < m_applications.size()
        ? m_applications.at(row).desktopId : QString();
}

bool OpenWithModel::setDefault(int row)
{
    if (row < 0 || row >= m_applications.size() || m_process)
        return false;
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("gio"));
    if (executable.isEmpty())
        return false;
    const QString desktopId = m_applications.at(row).desktopId;
    setLoading(true);
    auto *process = new QProcess(this);
    m_process = process;
    connect(process, &QProcess::finished, this,
            [this, process, desktopId](int exitCode, QProcess::ExitStatus exitStatus) {
                if (process != m_process) {
                    process->deleteLater();
                    return;
                }
                m_process = nullptr;
                const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
                if (success) {
                    for (ApplicationEntry &application : m_applications)
                        application.isDefault = application.desktopId == desktopId;
                    if (!m_applications.isEmpty())
                        emit dataChanged(index(0), index(m_applications.size() - 1), {DefaultRole});
                }
                QString message = success
                    ? QStringLiteral("Default application updated.")
                    : QString::fromUtf8(process->readAllStandardError()).trimmed();
                if (message.isEmpty())
                    message = QStringLiteral("Could not update the default application.");
                process->deleteLater();
                setLoading(false);
                emit defaultChanged(success, message);
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError processError) {
                if (processError != QProcess::FailedToStart || process != m_process)
                    return;
                m_process = nullptr;
                const QString message = process->errorString();
                process->deleteLater();
                setLoading(false);
                emit defaultChanged(false, message);
            });
    process->start(executable, {QStringLiteral("mime"), m_mimeType, desktopId},
                   QIODevice::ReadOnly);
    return true;
}

void OpenWithModel::stopProcess()
{
    if (!m_process)
        return;
    QProcess *process = m_process;
    m_process = nullptr;
    process->disconnect(this);
    process->kill();
    process->deleteLater();
}

void OpenWithModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void OpenWithModel::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message)
        return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

void OpenWithModel::replaceApplications(QVector<ApplicationEntry> applications)
{
    beginResetModel();
    m_applications = std::move(applications);
    endResetModel();
    emit countChanged();
}
