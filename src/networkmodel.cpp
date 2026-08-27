#include "networkmodel.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include <unistd.h>

namespace {
QHash<QString, QString> mountAttributes(const QString &mountPath, QString &type)
{
    const QString name = QFileInfo(mountPath).fileName();
    const int separator = name.indexOf(QLatin1Char(':'));
    type = (separator < 0 ? name : name.left(separator)).toCaseFolded();
    QHash<QString, QString> attributes;
    const QString encodedAttributes = separator < 0 ? QString() : name.mid(separator + 1);
    for (const QString &part : encodedAttributes.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const int equals = part.indexOf(QLatin1Char('='));
        if (equals <= 0)
            continue;
        attributes.insert(part.left(equals).toCaseFolded(),
                          QUrl::fromPercentEncoding(part.mid(equals + 1).toUtf8()));
    }
    return attributes;
}

bool attributeEquals(const QHash<QString, QString> &attributes,
                     const QStringList &keys, const QString &wanted)
{
    for (const QString &key : keys) {
        const auto found = attributes.constFind(key);
        if (found != attributes.cend())
            return found.value().compare(wanted, Qt::CaseInsensitive) == 0;
    }
    return false;
}
}

NetworkModel::NetworkModel(QObject *parent)
    : QObject(parent)
{
}

NetworkModel::~NetworkModel()
{
    cancel();
}

bool NetworkModel::connecting() const { return m_connecting; }
bool NetworkModel::promptActive() const { return m_promptActive; }
bool NetworkModel::promptSecret() const { return m_promptSecret; }
QString NetworkModel::promptText() const { return m_promptText; }
QString NetworkModel::uri() const { return m_uri; }
QString NetworkModel::errorMessage() const { return m_errorMessage; }

QString NetworkModel::normalizedUri(const QString &uri)
{
    QUrl url = QUrl::fromUserInput(uri.trimmed());
    const QString scheme = url.scheme().toCaseFolded();
    static const QStringList allowed = {
        QStringLiteral("smb"), QStringLiteral("sftp"), QStringLiteral("dav"),
        QStringLiteral("davs"), QStringLiteral("nfs"),
    };
    if (!url.isValid() || !allowed.contains(scheme) || url.host().isEmpty())
        return {};
    url.setScheme(scheme);
    url.setPassword({});
    return url.toString(QUrl::FullyEncoded);
}

QStringList NetworkModel::mountedPaths()
{
    const QString root = qEnvironmentVariable("SHIBUI_GVFS_ROOT",
                                               QStringLiteral("/run/user/%1/gvfs").arg(getuid()));
    QDir directory(root);
    QStringList paths;
    for (const QFileInfo &entry : directory.entryInfoList(
             QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable))
        paths << entry.absoluteFilePath();
    return paths;
}

bool NetworkModel::connectTo(const QString &uri)
{
    if (m_process)
        return false;
    const QString normalized = normalizedUri(uri);
    if (normalized.isEmpty()) {
        m_errorMessage = QStringLiteral("Use an SMB, SFTP, WebDAV, or NFS address with a host.");
        emit changed();
        return false;
    }
    if (QStandardPaths::findExecutable(QStringLiteral("gio")).isEmpty()) {
        m_errorMessage = QStringLiteral("The gio command is unavailable.");
        emit changed();
        return false;
    }
    m_uri = normalized;
    m_errorMessage.clear();
    m_promptText.clear();
    m_promptActive = false;
    m_promptSecret = false;
    m_transcript.clear();
    m_promptBuffer.clear();
    const QStringList before = mountedPaths();
    m_mountsBefore = QSet<QString>(before.cbegin(), before.cend());
    const QString existingPath = locateMountedPath();
    if (!existingPath.isEmpty()) {
        saveRecentUri();
        const QString message = QStringLiteral("Connected to %1").arg(m_uri);
        emit changed();
        emit connected(existingPath, m_uri, message);
        emit finished(true, message);
        return true;
    }
    startProcess({QStringLiteral("mount"), m_uri}, false);
    return true;
}

void NetworkModel::submitResponse(const QString &response)
{
    if (!m_process || !m_promptActive)
        return;
    m_process->write(response.toUtf8());
    m_process->write("\n");
    m_promptActive = false;
    m_promptSecret = false;
    m_promptText.clear();
    m_promptBuffer.clear();
    emit changed();
}

void NetworkModel::cancel()
{
    if (!m_process)
        return;
    QProcess *process = m_process;
    m_process = nullptr;
    process->disconnect(this);
    process->kill();
    process->deleteLater();
    m_connecting = false;
    m_promptActive = false;
    emit changed();
}

bool NetworkModel::disconnectFrom(const QString &uri)
{
    if (m_process)
        return false;
    const QString normalized = normalizedUri(uri);
    if (normalized.isEmpty())
        return false;
    if (QStandardPaths::findExecutable(QStringLiteral("gio")).isEmpty()) {
        m_errorMessage = QStringLiteral("The gio command is unavailable.");
        emit changed();
        return false;
    }
    m_uri = normalized;
    m_errorMessage.clear();
    m_transcript.clear();
    startProcess({QStringLiteral("mount"), QStringLiteral("--unmount"), m_uri}, true);
    return true;
}

QString NetworkModel::locateMountedPath() const
{
    const QUrl url(m_uri);
    const QStringList after = mountedPaths();
    const QString scheme = url.scheme().toCaseFolded();
    const QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    auto exactMatch = [&](const QString &path) -> QString {
        QString mountType;
        const QHash<QString, QString> attributes = mountAttributes(path, mountType);
        const bool typeMatches = scheme == QStringLiteral("smb")
            ? mountType == QStringLiteral("smb-share")
            : scheme == QStringLiteral("sftp")
                ? mountType == QStringLiteral("sftp")
                : (scheme == QStringLiteral("dav") || scheme == QStringLiteral("davs"))
                    ? mountType.startsWith(QStringLiteral("dav"))
                    : scheme == QStringLiteral("nfs") && mountType == QStringLiteral("nfs");
        if (!typeMatches
            || !attributeEquals(attributes, {QStringLiteral("host"), QStringLiteral("server")},
                                url.host()))
            return {};
        if (!url.userName().isEmpty()
            && !attributeEquals(attributes, {QStringLiteral("user")}, url.userName()))
            return {};
        if (url.port() >= 0
            && !attributeEquals(attributes, {QStringLiteral("port")},
                                QString::number(url.port())))
            return {};

        QStringList relativeSegments = segments;
        if (scheme == QStringLiteral("smb")) {
            if (segments.isEmpty()
                || !attributeEquals(attributes, {QStringLiteral("share")}, segments.constFirst()))
                return {};
            relativeSegments.removeFirst();
        } else if (scheme == QStringLiteral("nfs")) {
            const QString requestedExport = QLatin1Char('/') + segments.join(QLatin1Char('/'));
            if (!attributeEquals(attributes, {QStringLiteral("path")}, requestedExport))
                return {};
            relativeSegments.clear();
        } else if ((scheme == QStringLiteral("dav") || scheme == QStringLiteral("davs"))
                   && attributes.contains(QStringLiteral("prefix"))) {
            const QString requestedPrefix = QLatin1Char('/') + segments.join(QLatin1Char('/'));
            if (QDir::cleanPath(attributes.value(QStringLiteral("prefix")))
                != QDir::cleanPath(requestedPrefix))
                return {};
            relativeSegments.clear();
        }

        if (relativeSegments.isEmpty())
            return path;
        const QString candidate = QDir(path).filePath(relativeSegments.join(QLatin1Char('/')));
        return QFileInfo(candidate).exists() ? QDir::cleanPath(candidate) : path;
    };

    for (const QString &path : after) {
        if (!m_mountsBefore.contains(path)) {
            const QString match = exactMatch(path);
            if (!match.isEmpty())
                return match;
        }
    }
    for (const QString &path : after) {
        const QString match = exactMatch(path);
        if (!match.isEmpty())
            return match;
    }
    return {};
}

void NetworkModel::startProcess(const QStringList &arguments, bool disconnecting)
{
    auto *process = new QProcess(this);
    m_process = process;
    m_connecting = true;
    m_disconnecting = disconnecting;
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::readyRead, this, &NetworkModel::consumeOutput);
    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                finishProcess(process,
                              exitStatus == QProcess::NormalExit && exitCode == 0);
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError processError) {
                if (processError != QProcess::FailedToStart || process != m_process)
                    return;
                m_transcript += process->errorString();
                finishProcess(process, false);
            });
    process->start(QStandardPaths::findExecutable(QStringLiteral("gio")), arguments,
                   QIODevice::ReadWrite);
    emit changed();
}

void NetworkModel::finishProcess(QProcess *process, bool success)
{
    if (process != m_process) {
        process->deleteLater();
        return;
    }
    consumeOutput();
    m_process = nullptr;
    m_connecting = false;
    m_promptActive = false;
    m_promptBuffer.clear();
    QString message = m_transcript.trimmed();
    if (success && !m_disconnecting) {
        const QString path = locateMountedPath();
        if (path.isEmpty()) {
            m_errorMessage = QStringLiteral("The share mounted, but its GVfs path is unavailable.");
            emit changed();
            emit finished(false, m_errorMessage);
        } else {
            saveRecentUri();
            if (message.isEmpty())
                message = QStringLiteral("Connected to %1").arg(m_uri);
            emit changed();
            emit connected(path, m_uri, message);
            emit finished(true, message);
        }
    } else {
        if (message.isEmpty())
            message = success ? QStringLiteral("Disconnected from %1").arg(m_uri)
                              : QStringLiteral("Could not connect to %1").arg(m_uri);
        m_errorMessage = success ? QString() : message;
        emit changed();
        emit finished(success, message);
    }
    process->deleteLater();
}

void NetworkModel::consumeOutput()
{
    if (!m_process)
        return;
    const QString chunk = QString::fromUtf8(m_process->readAll());
    if (chunk.isEmpty())
        return;
    m_transcript += chunk;
    m_promptBuffer += chunk;
    const int newline = qMax(m_promptBuffer.lastIndexOf(QLatin1Char('\n')),
                             m_promptBuffer.lastIndexOf(QLatin1Char('\r')));
    if (newline >= 0)
        m_promptBuffer = m_promptBuffer.mid(newline + 1);
    const QString prompt = m_promptBuffer.trimmed();
    if (!prompt.isEmpty() && (prompt.endsWith(QLatin1Char(':'))
                              || prompt.endsWith(QLatin1Char('?')))) {
        const QString lower = prompt.toCaseFolded();
        m_promptText = prompt.section(QLatin1Char('\n'), -1);
        m_promptSecret = lower.contains(QStringLiteral("password"))
            || lower.contains(QStringLiteral("passphrase"));
        m_promptActive = true;
        emit changed();
    }
}

void NetworkModel::saveRecentUri()
{
    QSettings settings;
    QStringList recents = settings.value(QStringLiteral("network/recent")).toStringList();
    recents.removeAll(m_uri);
    recents.prepend(m_uri);
    while (recents.size() > 8)
        recents.removeLast();
    settings.setValue(QStringLiteral("network/recent"), recents);
}
