#include "fileiconprovider.h"
#include "filesystemmodel.h"
#include "placesmodel.h"
#include "searchmodel.h"
#include "previewmodel.h"
#include "openwithmodel.h"
#include "propertiesmodel.h"
#include "networkmodel.h"
#include "archivemodel.h"
#include "recentmodel.h"
#include "bulkrenamemodel.h"
#include "templatemodel.h"
#include "thememanager.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QStandardPaths>

#include <unistd.h>

namespace {
bool isNetworkLocation(const QString &location)
{
    const QString scheme = QUrl(location).scheme().toCaseFolded();
    return scheme == QStringLiteral("smb") || scheme == QStringLiteral("sftp")
        || scheme == QStringLiteral("dav") || scheme == QStringLiteral("davs")
        || scheme == QStringLiteral("nfs");
}

QString localPathForArgument(const QString &argument)
{
    const QUrl url(argument);
    return url.isLocalFile() ? url.toLocalFile() : argument;
}

bool sendToExistingInstance(const QString &instanceName,
                            const QString &requestedLocation, int timeoutMilliseconds)
{
    QLocalSocket socket;
    socket.connectToServer(instanceName);
    if (!socket.waitForConnected(timeoutMilliseconds))
        return false;
    socket.write(requestedLocation.toUtf8() + '\n');
    socket.waitForBytesWritten(500);
    return true;
}
}

int main(int argc, char *argv[])
{
    QElapsedTimer startupTimer;
    startupTimer.start();

    QCoreApplication::setApplicationName(QStringLiteral("Shibui"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.1"));
    QCoreApplication::setOrganizationName(QStringLiteral("Shibui"));
    QGuiApplication::setDesktopFileName(QStringLiteral("shibui"));

    QApplication application(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("A fast, keyboard-first file manager for Omarchy."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption profileStartupOption(
        QStringLiteral("profile-startup"),
        QStringLiteral("Print time until the UI and initial directory are ready, then exit."));
    parser.addOption(profileStartupOption);
    parser.addPositionalArgument(QStringLiteral("path"),
                                 QStringLiteral("Local path or network URI to open."),
                                 QStringLiteral("[location]"));
    parser.process(application);
    const bool profileStartup = parser.isSet(profileStartupOption);

    const QString requestedLocation = parser.positionalArguments().value(0);
    const QString instanceName = QStringLiteral("shibui-%1").arg(getuid());
    if (sendToExistingInstance(instanceName, requestedLocation, 200))
        return 0;

    QString runtimePath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimePath.isEmpty())
        runtimePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QLockFile instanceLock(QDir(runtimePath).filePath(instanceName + QStringLiteral(".lock")));
    if (!instanceLock.tryLock()) {
        QElapsedTimer waitForPrimary;
        waitForPrimary.start();
        while (waitForPrimary.elapsed() < 2000) {
            if (sendToExistingInstance(instanceName, requestedLocation, 100))
                return 0;
            QThread::msleep(25);
        }
        QTextStream(stderr) << "shibui: another instance owns the startup lock but is unavailable\n";
        return 1;
    }

    QLocalServer instanceServer;
    instanceServer.setSocketOptions(QLocalServer::UserAccessOption);
    if (!instanceServer.listen(instanceName)) {
        QLocalServer::removeServer(instanceName);
        if (!instanceServer.listen(instanceName)) {
            QTextStream(stderr) << "shibui: single-instance socket unavailable: "
                                << instanceServer.errorString() << '\n';
            return 1;
        }
    }

    ThemeManager theme;
    FileSystemModel files;
    PlacesModel places;
    SearchModel search;
    PreviewModel preview;
    OpenWithModel openWith;
    PropertiesModel properties;
    NetworkModel network;
    ArchiveModel archive;
    RecentModel recent;
    BulkRenameModel bulkRename;
    TemplateModel templates;
    QObject::connect(&theme, &ThemeManager::revisionChanged, &files,
                     [&theme, &files] { files.setIconRevision(theme.revision()); });
    files.setIconRevision(theme.revision());

    QString initialPath = QDir::homePath();
    QString initialSelection;
    QString initialNetworkLocation;
    if (!requestedLocation.isEmpty() && isNetworkLocation(requestedLocation)) {
        initialNetworkLocation = requestedLocation;
    } else if (!requestedLocation.isEmpty()) {
        const QFileInfo requested(localPathForArgument(requestedLocation));
        if (requested.exists() && requested.isFile()) {
            initialPath = requested.absolutePath();
            initialSelection = requested.absoluteFilePath();
        } else {
            initialPath = requested.absoluteFilePath();
        }
    }
    if (!files.navigateTo(initialPath))
        files.navigateTo(QDir::homePath());

    bool startupUiReady = false;
    bool startupFilesReady = !files.loading();
    bool startupReported = false;
    auto reportStartupWhenReady = [&] {
        if (!profileStartup || startupReported || !startupUiReady
            || !startupFilesReady)
            return;
        startupReported = true;
        QTextStream(stderr) << "shibui-ready-ms=" << startupTimer.elapsed() << '\n';
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    };
    if (profileStartup) {
        QObject::connect(&files, &FileSystemModel::loadingChanged, &application, [&] {
            startupFilesReady = !files.loading();
            reportStartupWhenReady();
        });
    }

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("fileicon"), new FileIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("fileModel"), &files);
    engine.rootContext()->setContextProperty(QStringLiteral("placesModel"), &places);
    engine.rootContext()->setContextProperty(QStringLiteral("searchModel"), &search);
    engine.rootContext()->setContextProperty(QStringLiteral("previewModel"), &preview);
    engine.rootContext()->setContextProperty(QStringLiteral("openWithModel"), &openWith);
    engine.rootContext()->setContextProperty(QStringLiteral("propertiesModel"), &properties);
    engine.rootContext()->setContextProperty(QStringLiteral("networkModel"), &network);
    engine.rootContext()->setContextProperty(QStringLiteral("archiveModel"), &archive);
    engine.rootContext()->setContextProperty(QStringLiteral("recentModel"), &recent);
    engine.rootContext()->setContextProperty(QStringLiteral("bulkRenameModel"), &bulkRename);
    engine.rootContext()->setContextProperty(QStringLiteral("templateModel"), &templates);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("initialSelectionPath"), initialSelection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    startupUiReady = true;
    reportStartupWhenReady();

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    auto openLocation = [&files, &network, window](const QString &rawLocation) {
        if (window) {
            window->show();
            window->raise();
            window->requestActivate();
        }
        if (rawLocation.isEmpty())
            return;
        if (isNetworkLocation(rawLocation)) {
            network.connectTo(rawLocation);
            return;
        }
        const QFileInfo requested(localPathForArgument(rawLocation));
        const QString directory = requested.exists() && requested.isFile()
            ? requested.absolutePath() : requested.absoluteFilePath();
        const QString selection = requested.exists() && requested.isFile()
            ? requested.absoluteFilePath() : QString();
        if (window) {
            QMetaObject::invokeMethod(window, "openExternalLocation",
                                      Q_ARG(QVariant, directory),
                                      Q_ARG(QVariant, selection));
        } else {
            files.navigateTo(directory);
        }
    };

    QObject::connect(&instanceServer, &QLocalServer::newConnection, &application,
                     [&instanceServer, &openLocation] {
        while (QLocalSocket *socket = instanceServer.nextPendingConnection()) {
            socket->setParent(&instanceServer);
            auto consume = [socket, &openLocation] {
                QByteArray buffer = socket->property("shibuiBuffer").toByteArray();
                buffer += socket->readAll();
                qsizetype newline = -1;
                while ((newline = buffer.indexOf('\n')) >= 0) {
                    openLocation(QString::fromUtf8(buffer.first(newline)));
                    buffer.remove(0, newline + 1);
                }
                socket->setProperty("shibuiBuffer", buffer);
            };
            QObject::connect(socket, &QLocalSocket::readyRead, socket, consume);
            if (socket->bytesAvailable() > 0)
                consume();
        }
    });
    if (!initialNetworkLocation.isEmpty())
        QTimer::singleShot(0, &application,
                           [&openLocation, initialNetworkLocation] {
            openLocation(initialNetworkLocation);
        });

    return application.exec();
}
