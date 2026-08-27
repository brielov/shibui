#include "filesystemmodel.h"
#include "fileiconprovider.h"
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

#include <QDir>
#include <QClipboard>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QMimeData>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include <memory>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>

namespace {
void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(file.errorString()));
    QCOMPARE(file.write(contents), contents.size());
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

QString findPathEndingWith(const FileSystemModel &model, const QString &suffix)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        const QString path = model.data(model.index(row), FileSystemModel::PathRole).toString();
        if (path.endsWith(suffix))
            return path;
    }
    return {};
}

QQuickItem *findQuickItem(QQuickItem *parent, const QString &objectName)
{
    if (!parent)
        return nullptr;
    if (parent->objectName() == objectName)
        return parent;
    for (QQuickItem *child : parent->childItems()) {
        if (QQuickItem *match = findQuickItem(child, objectName))
            return match;
    }
    return nullptr;
}

void typeText(QWindow *window, const QString &text)
{
    for (const QChar character : text) {
        if (character.isLetter()) {
            const int offset = character.toLower().unicode() - QLatin1Char('a').unicode();
            QTest::keyClick(window, static_cast<Qt::Key>(Qt::Key_A + offset));
        } else if (character == QLatin1Char('-')) {
            QTest::keyClick(window, Qt::Key_Minus);
        } else if (character == QLatin1Char('.')) {
            QTest::keyClick(window, Qt::Key_Period);
        } else if (character == QLatin1Char(' ')) {
            QTest::keyClick(window, Qt::Key_Space);
        }
    }
}
}

class CoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void loadsThemeTokensAndWatchesChanges();
    void browsesFiltersAndShowsHiddenFiles();
    void emptyDirectoriesFinishLoading();
    void keepsCachedHiddenEntriesOutOfNormalView();
    void followsExternalDirectoryChanges();
    void reportsWhenTheCurrentDirectoryDisappears();
    void createsAndRenamesItemsSafely();
    void copiesMovesCancelsAndResolvesConflicts();
    void protectsDestructiveTransfersFromFilesystemRaces();
    void movesItemsToTrashAndReportsPartialFailures();
    void aggregatesTrashLocations();
    void undoesRecentMutationsSafely();
    void interoperatesWithDesktopClipboard();
    void listsStandardPlacesAndMountedVolumes();
    void recursivelyFindsAndFuzzyRanksPaths();
    void previewsCommonLocalFilesAsynchronously();
    void listsCompatibleApplications();
    void reportsPropertiesAndCalculatesFolderSize();
    void validatesNetworkLocationsWithoutStoringCredentials();
    void matchesExactNetworkSharesAndMasksSplitPrompts();
    void subprocessStartFailuresClearActiveState();
    void createsAndExtractsCommonArchives();
    void readsDesktopRecentFilesWithoutKeepingAnotherHistory();
    void previewsAndAppliesBulkRenames();
    void createsDocumentsFromTheStandardTemplatesFolder();
    void createsAnUndoableFolderFromTheSelection();
    void keyboardContractDrivesTheRealInterface();
    void largeDirectoryLoadsAndFiltersPromptly();

private:
    std::unique_ptr<QTemporaryDir> m_testDataDirectory;
};

void CoreTest::initTestCase()
{
    qputenv("SHIBUI_SKIP_DESKTOP_QUERIES", "1");
    m_testDataDirectory = std::make_unique<QTemporaryDir>(
        QDir::current().filePath(QStringLiteral(".shibui-test-data-XXXXXX")));
    QVERIFY(m_testDataDirectory->isValid());
    qputenv("XDG_DATA_HOME", m_testDataDirectory->path().toUtf8());
    qputenv("XDG_CONFIG_HOME", m_testDataDirectory->path().toUtf8());

    const QString applicationsDirectory = QDir(m_testDataDirectory->path())
                                              .filePath(QStringLiteral("applications"));
    QVERIFY(QDir().mkpath(applicationsDirectory));
    writeFile(QDir(applicationsDirectory).filePath(QStringLiteral("shibui-test.desktop")), R"(
[Desktop Entry]
Type=Application
Name=Shibui Test Viewer
Exec=/usr/bin/true %f
MimeType=text/plain;
NoDisplay=true
)");
    writeFile(QDir(m_testDataDirectory->path()).filePath(QStringLiteral("mimeapps.list")), R"(
[Default Applications]
text/plain=shibui-test.desktop;

[Added Associations]
text/plain=shibui-test.desktop;
)");
}

void CoreTest::loadsThemeTokensAndWatchesChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    writeFile(directory.filePath(QStringLiteral("colors.toml")), R"(
mode = "light"
background = "#102030"
dark_background = "#081018"
lighter_background = "#203040"
foreground = "#f0e0d0"
muted = "#cecacd"
dark_foreground = "#9893a5"
light_foreground = "#6e6a86"
accent = "#44aa88"
selection = "#224438"
red = "#ee4455"
)");
    writeFile(directory.filePath(QStringLiteral("shell.toml")), R"(
[font]
base-size = 14

[spacing]
scale = 1.25
scale-with-font = false

[controls]
hover-cursor-fill-alpha = 0.12
selected-fill-alpha = 0.24
hover-cursor-border-alpha = 0.5
)");
    writeFile(directory.filePath(QStringLiteral("icons.theme")), "hicolor\n");
    writeFile(directory.filePath(QStringLiteral("theme.name")), "test-theme\n");

    ThemeManager theme(directory.path());
    QCOMPARE(theme.background(), QColor(QStringLiteral("#102030")));
    QCOMPARE(theme.muted(), QColor(QStringLiteral("#6e6a86")));
    QCOMPARE(theme.accent(), QColor(QStringLiteral("#44aa88")));
    QCOMPARE(theme.fontSize(), 14.0);
    QCOMPARE(theme.effectiveSpacingScale(), 1.25);
    QCOMPARE(theme.hoverFillAlpha(), 0.12);
    QCOMPARE(theme.selectedFillAlpha(), 0.24);
    QCOMPARE(theme.hoverBorderAlpha(), 0.5);
    QCOMPARE(theme.iconThemeName(), QStringLiteral("hicolor"));
    QCOMPARE(theme.themeName(), QStringLiteral("test-theme"));

    FileIconProvider icons;
    QSize iconSize;
    const QPixmap placeIcon = icons.requestPixmap(
        QStringLiteral("theme/user-home-symbolic/607080"), &iconSize, QSize(16, 16));
    QVERIFY(!placeIcon.isNull());
    QCOMPARE(iconSize, QSize(16, 16));

    const int oldRevision = theme.revision();
    writeFile(directory.filePath(QStringLiteral("colors.toml")), R"(
mode = "dark"
background = "#334455"
muted = "#607080"
accent = "#abcdef"
)");
    QTRY_VERIFY_WITH_TIMEOUT(theme.revision() > oldRevision, 2000);
    QCOMPARE(theme.background(), QColor(QStringLiteral("#334455")));
    QCOMPARE(theme.muted(), QColor(QStringLiteral("#607080")));
    QCOMPARE(theme.accent(), QColor(QStringLiteral("#abcdef")));
}

void CoreTest::browsesFiltersAndShowsHiddenFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("folder")));
    writeFile(directory.filePath(QStringLiteral("alpha.txt")), "alpha");
    writeFile(directory.filePath(QStringLiteral("beta.bin")), "beta");
    writeFile(directory.filePath(QStringLiteral(".secret")), "hidden");
    QVERIFY(QFile::link(directory.filePath(QStringLiteral("missing")),
                        directory.filePath(QStringLiteral("broken-link"))));

    FileSystemModel model;
    QVERIFY(model.navigateTo(directory.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 4, 3000);

    const QFileInfo first(model.pathAt(0));
    QVERIFY2(first.isDir(), "Directories must sort before files");
    QVERIFY(findPathEndingWith(model, QStringLiteral("alpha.txt")).endsWith(QStringLiteral("alpha.txt")));
    QVERIFY(findPathEndingWith(model, QStringLiteral("broken-link")).endsWith(QStringLiteral("broken-link")));

    const int brokenRow = model.indexOfPath(directory.filePath(QStringLiteral("broken-link")));
    QVERIFY(brokenRow >= 0);
    QVERIFY(model.data(model.index(brokenRow), FileSystemModel::BrokenSymlinkRole).toBool());

    model.setFilterText(QStringLiteral("beta"));
    QTRY_COMPARE(model.rowCount(), 1);
    QVERIFY(model.pathAt(0).endsWith(QStringLiteral("beta.bin")));

    model.setFilterText({});
    model.setShowHidden(true);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 5, 3000);
    QVERIFY(findPathEndingWith(model, QStringLiteral(".secret")).endsWith(QStringLiteral(".secret")));
}

void CoreTest::emptyDirectoriesFinishLoading()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FileSystemModel model;
    QVERIFY(model.navigateTo(directory.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QCOMPARE(model.rowCount(), 0);
    model.setShowHidden(true);
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QCOMPARE(model.rowCount(), 0);
}

void CoreTest::keepsCachedHiddenEntriesOutOfNormalView()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QVERIFY(QDir(first.path()).mkdir(QStringLiteral(".local")));
    QVERIFY(QDir(first.path()).mkdir(QStringLiteral("Documents")));
    QVERIFY(QDir(second.path()).mkdir(QStringLiteral("elsewhere")));

    FileSystemModel model;
    model.setShowHidden(true);
    QVERIFY(model.navigateTo(first.path()));
    QTRY_VERIFY_WITH_TIMEOUT(model.indexOfPath(first.filePath(QStringLiteral(".local"))) >= 0,
                             3000);
    QVERIFY(model.navigateTo(second.path()));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 3000);
    model.setShowHidden(false);
    QVERIFY(model.navigateTo(first.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 3000);
    QCOMPARE(model.indexOfPath(first.filePath(QStringLiteral(".local"))), -1);
    QCOMPARE(model.pathAt(0), first.filePath(QStringLiteral("Documents")));

    const QString desktopLocal = QDir::home().filePath(QStringLiteral(".local"));
    if (QFileInfo(desktopLocal).isDir()) {
        QVERIFY(model.navigateTo(QDir::homePath()));
        QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
        QCOMPARE(model.indexOfPath(desktopLocal), -1);
    }
}

void CoreTest::followsExternalDirectoryChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeFile(directory.filePath(QStringLiteral("before.txt")), "before");

    FileSystemModel model;
    QVERIFY(model.navigateTo(directory.path()));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 3000);

    writeFile(directory.filePath(QStringLiteral("added.txt")), "added");
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 2, 3000);

    QVERIFY(QFile::rename(directory.filePath(QStringLiteral("added.txt")),
                          directory.filePath(QStringLiteral("renamed.txt"))));
    QTRY_VERIFY_WITH_TIMEOUT(model.indexOfPath(directory.filePath(QStringLiteral("renamed.txt"))) >= 0, 3000);
    QCOMPARE(model.indexOfPath(directory.filePath(QStringLiteral("added.txt"))), -1);

    QVERIFY(QFile::remove(directory.filePath(QStringLiteral("before.txt"))));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 3000);
}

void CoreTest::reportsWhenTheCurrentDirectoryDisappears()
{
    QTemporaryDir parent;
    QVERIFY(parent.isValid());
    const QString child = parent.filePath(QStringLiteral("child"));
    QVERIFY(QDir(parent.path()).mkdir(QStringLiteral("child")));
    writeFile(QDir(child).filePath(QStringLiteral("item.txt")), "item");

    FileSystemModel model;
    QVERIFY(model.navigateTo(child));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 3000);

    QVERIFY(QFile::remove(QDir(child).filePath(QStringLiteral("item.txt"))));
    QVERIFY(QDir(parent.path()).rmdir(QStringLiteral("child")));
    QTRY_VERIFY_WITH_TIMEOUT(model.errorMessage().contains(QStringLiteral("no longer exists")), 3000);
    QCOMPARE(model.rowCount(), 0);
}

void CoreTest::createsAndRenamesItemsSafely()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeFile(directory.filePath(QStringLiteral("existing.txt")), "existing");

    FileSystemModel model;
    QVERIFY(model.navigateTo(directory.path()));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 3000);

    const QString createdPath = model.createDirectory(QStringLiteral("New Folder"));
    QCOMPARE(createdPath, directory.filePath(QStringLiteral("New Folder")));
    QVERIFY(QFileInfo(createdPath).isDir());
    QTRY_VERIFY_WITH_TIMEOUT(model.indexOfPath(createdPath) >= 0, 3000);

    QCOMPARE(model.createDirectory(QStringLiteral("existing.txt")), QString());
    QVERIFY(model.errorMessage().contains(QStringLiteral("already exists")));
    QCOMPARE(model.createDirectory(QStringLiteral("nested/folder")), QString());
    QVERIFY(!QFileInfo::exists(directory.filePath(QStringLiteral("nested"))));

    const QString renamedPath = model.renamePath(createdPath, QStringLiteral("Renamed Folder"));
    QCOMPARE(renamedPath, directory.filePath(QStringLiteral("Renamed Folder")));
    QVERIFY(QFileInfo(renamedPath).isDir());
    QVERIFY(!QFileInfo::exists(createdPath));
    QTRY_VERIFY_WITH_TIMEOUT(model.indexOfPath(renamedPath) >= 0, 3000);

    QCOMPARE(model.renamePath(renamedPath, QStringLiteral("existing.txt")), QString());
    QVERIFY(model.errorMessage().contains(QStringLiteral("already exists")));
    QVERIFY(QFileInfo(renamedPath).isDir());
}

void CoreTest::copiesMovesCancelsAndResolvesConflicts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = directory.filePath(QStringLiteral("source"));
    const QString destination = directory.filePath(QStringLiteral("destination"));
    QVERIFY(QDir().mkpath(QDir(source).filePath(QStringLiteral("folder/nested"))));
    QVERIFY(QDir().mkpath(destination));
    writeFile(QDir(source).filePath(QStringLiteral("folder/nested/data.txt")), "nested data");
    writeFile(QDir(source).filePath(QStringLiteral("single.txt")), "single data");
    QVERIFY(QFile::link(QDir(source).filePath(QStringLiteral("folder/missing")),
                        QDir(source).filePath(QStringLiteral("folder/broken-link"))));

    FileSystemModel model;
    QVERIFY(model.navigateTo(destination));
    QSignalSpy finished(&model, &FileSystemModel::transferFinished);

    QVERIFY(model.startTransfer({QDir(source).filePath(QStringLiteral("folder")),
                                 QDir(source).filePath(QStringLiteral("single.txt"))},
                                destination, false));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QCOMPARE(finished.size(), 1);
    QVERIFY(finished.takeFirst().at(0).toBool());
    QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("folder/nested/data.txt"))),
             QByteArray("nested data"));
    QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("single.txt"))),
             QByteArray("single data"));
    QVERIFY(QFileInfo(QDir(destination).filePath(QStringLiteral("folder/broken-link"))).isSymLink());
    QVERIFY(QFileInfo::exists(QDir(source).filePath(QStringLiteral("single.txt"))));

    QVERIFY(model.startTransfer({QDir(destination).filePath(QStringLiteral("single.txt"))},
                                destination, false));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QVERIFY(finished.takeFirst().at(0).toBool());
    QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("single (copy).txt"))),
             QByteArray("single data"));

    const QString moveSource = QDir(source).filePath(QStringLiteral("move-me.txt"));
    writeFile(moveSource, "move data");
    QVERIFY(model.startTransfer({moveSource}, destination, true));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QVERIFY(finished.takeFirst().at(0).toBool());
    QVERIFY(!QFileInfo::exists(moveSource));
    QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("move-me.txt"))),
             QByteArray("move data"));

    const QString collidingSource = QDir(source).filePath(QStringLiteral("single.txt"));
    QVERIFY(model.startTransfer({collidingSource}, destination, false));
    QTRY_VERIFY_WITH_TIMEOUT(model.transferConflictActive(), 3000);
    QVERIFY(model.resolveTransferConflict(QStringLiteral("skip"), {}, false));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QVERIFY(finished.takeFirst().at(0).toBool());
    QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("single.txt"))),
             QByteArray("single data"));

    writeFile(collidingSource, "replacement data");
    QVERIFY(model.startTransfer({collidingSource}, destination, false));
    QTRY_VERIFY_WITH_TIMEOUT(model.transferConflictActive(), 3000);
    QVERIFY(model.resolveTransferConflict(QStringLiteral("replace"), {}, false));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QVERIFY(finished.takeFirst().at(0).toBool());
    QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("single.txt"))),
             QByteArray("replacement data"));
    QCOMPARE(readFile(collidingSource), QByteArray("replacement data"));

    QVERIFY(model.startTransfer({collidingSource}, destination, false));
    QTRY_VERIFY_WITH_TIMEOUT(model.transferConflictActive(), 3000);
    QVERIFY(!model.resolveTransferConflict(QStringLiteral("rename"),
                                           QStringLiteral("single.txt"), false));
    QVERIFY(model.transferConflictError().contains(QStringLiteral("already exists")));
    QVERIFY(model.resolveTransferConflict(QStringLiteral("rename"),
                                          QStringLiteral("renamed.txt"), false));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QVERIFY(finished.takeFirst().at(0).toBool());
    QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("renamed.txt"))),
             QByteArray("replacement data"));

    QVERIFY(model.startTransfer({collidingSource}, destination, false));
    QTRY_VERIFY_WITH_TIMEOUT(model.transferConflictActive(), 3000);
    QVERIFY(model.resolveTransferConflict(QStringLiteral("cancel"), {}, false));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QVERIFY(finished.takeFirst().at(1).toBool());

    const QString secondSource = QDir(source).filePath(QStringLiteral("second.txt"));
    writeFile(secondSource, "source second");
    writeFile(QDir(destination).filePath(QStringLiteral("second.txt")), "destination second");
    QVERIFY(model.startTransfer({collidingSource, secondSource}, destination, false));
    QTRY_VERIFY_WITH_TIMEOUT(model.transferConflictActive(), 3000);
    QVERIFY(model.resolveTransferConflict(QStringLiteral("skip"), {}, true));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QVERIFY(finished.takeFirst().at(0).toBool());
    QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("single.txt"))),
             QByteArray("replacement data"));
    QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("second.txt"))),
             QByteArray("destination second"));

    writeFile(QDir(destination).filePath(QStringLiteral("folder/old-only.txt")), "old");
    QVERIFY(model.startTransfer({QDir(source).filePath(QStringLiteral("folder"))},
                                destination, false));
    QTRY_VERIFY_WITH_TIMEOUT(model.transferConflictActive(), 3000);
    QVERIFY(model.transferConflictSourceIsDirectory());
    QVERIFY(model.transferConflictTargetIsDirectory());
    QVERIFY(model.resolveTransferConflict(QStringLiteral("replace"), {}, false));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QVERIFY(finished.takeFirst().at(0).toBool());
    QVERIFY(!QFileInfo::exists(QDir(destination).filePath(QStringLiteral("folder/old-only.txt"))));
    QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("folder/nested/data.txt"))),
             QByteArray("nested data"));

    const QString largeSource = QDir(source).filePath(QStringLiteral("large.bin"));
    QFile large(largeSource);
    QVERIFY(large.open(QIODevice::WriteOnly));
    QVERIFY(large.resize(256 * 1024 * 1024));
    large.close();
    QVERIFY(model.startTransfer({largeSource}, destination, false));
    model.cancelTransfer();
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    const QList<QVariant> cancelledResult = finished.takeFirst();
    QVERIFY(cancelledResult.at(1).toBool());
    QVERIFY(!QFileInfo::exists(QDir(destination).filePath(QStringLiteral("large.bin"))));

    const QString blockingSource = QDir(source).filePath(QStringLiteral("blocking.pipe"));
    QCOMPARE(::mkfifo(QFile::encodeName(blockingSource).constData(), 0600), 0);
    QVERIFY(model.startTransfer({blockingSource}, destination, false));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 3000);
    const QList<QVariant> unsupportedResult = finished.takeFirst();
    QVERIFY(!unsupportedResult.at(0).toBool());
    QVERIFY(unsupportedResult.at(2).toString().contains(QStringLiteral("Unsupported")));
    QVERIFY(QFileInfo::exists(blockingSource));
}

void CoreTest::protectsDestructiveTransfersFromFilesystemRaces()
{
    const auto cancelled = std::make_shared<std::atomic_bool>(false);
    const auto progress = [](const TransferUpdate &) {};

    QTemporaryDir aliasCase;
    QVERIFY(aliasCase.isValid());
    const QString aliasedSource = aliasCase.filePath(QStringLiteral("source"));
    const QString inner = QDir(aliasedSource).filePath(QStringLiteral("inner"));
    QVERIFY(QDir().mkpath(inner));
    writeFile(QDir(aliasedSource).filePath(QStringLiteral("payload.txt")), "payload");
    const QString alias = aliasCase.filePath(QStringLiteral("alias"));
    QVERIFY(QFile::link(inner, alias));
    const TransferResult aliased = runFileTransfer(
        {aliasedSource}, alias, true, cancelled, progress);
    QVERIFY(!aliased.success);
    QCOMPARE(readFile(QDir(aliasedSource).filePath(QStringLiteral("payload.txt"))),
             QByteArray("payload"));

    QTemporaryDir duplicateCase;
    QVERIFY(duplicateCase.isValid());
    const QString firstFolder = duplicateCase.filePath(QStringLiteral("first"));
    const QString secondFolder = duplicateCase.filePath(QStringLiteral("second"));
    const QString destination = duplicateCase.filePath(QStringLiteral("destination"));
    QVERIFY(QDir().mkpath(firstFolder));
    QVERIFY(QDir().mkpath(secondFolder));
    QVERIFY(QDir().mkpath(destination));
    const QString first = QDir(firstFolder).filePath(QStringLiteral("same.txt"));
    const QString second = QDir(secondFolder).filePath(QStringLiteral("same.txt"));
    writeFile(first, "first");
    writeFile(second, "second");
    const TransferResult duplicate = runFileTransfer(
        {first, second}, destination, true, cancelled, progress,
        [](const TransferConflict &) {
            return TransferConflictDecision{TransferConflictAction::Replace, {}, false};
        });
    QVERIFY(!duplicate.success);
    QCOMPARE(readFile(first), QByteArray("first"));
    QCOMPARE(readFile(second), QByteArray("second"));
    QVERIFY(!QFileInfo::exists(QDir(destination).filePath(QStringLiteral("same.txt"))));

    const QString restoreOne = duplicateCase.filePath(QStringLiteral("restore-one"));
    const QString restoreTwo = duplicateCase.filePath(QStringLiteral("restore-two"));
    const QString restoreTarget = duplicateCase.filePath(QStringLiteral("restored.txt"));
    const QString infoOne = duplicateCase.filePath(QStringLiteral("one.trashinfo"));
    const QString infoTwo = duplicateCase.filePath(QStringLiteral("two.trashinfo"));
    writeFile(restoreOne, "one");
    writeFile(restoreTwo, "two");
    writeFile(infoOne, "metadata");
    writeFile(infoTwo, "metadata");
    const TransferResult duplicateRestore = runFileRestore(
        {{restoreOne, restoreTarget, infoOne}, {restoreTwo, restoreTarget, infoTwo}},
        cancelled, progress,
        [](const TransferConflict &) {
            return TransferConflictDecision{TransferConflictAction::Replace, {}, false};
        });
    QVERIFY(!duplicateRestore.success);
    QCOMPARE(readFile(restoreOne), QByteArray("one"));
    QCOMPARE(readFile(restoreTwo), QByteArray("two"));
    QVERIFY(!QFileInfo::exists(restoreTarget));

    QTemporaryDir sourceRoot(QStringLiteral("/tmp/shibui-move-source-XXXXXX"));
    QTemporaryDir destinationRoot(QStringLiteral("/dev/shm/shibui-move-target-XXXXXX"));
    if (!sourceRoot.isValid() || !destinationRoot.isValid())
        QSKIP("A writable second filesystem is required for the cross-filesystem move test.");
    struct stat sourceStatus {};
    struct stat destinationStatus {};
    QVERIFY(::stat(QFile::encodeName(sourceRoot.path()).constData(), &sourceStatus) == 0);
    QVERIFY(::stat(QFile::encodeName(destinationRoot.path()).constData(), &destinationStatus) == 0);
    if (sourceStatus.st_dev == destinationStatus.st_dev)
        QSKIP("A second filesystem is required for the cross-filesystem move test.");

    const QString changingSource = sourceRoot.filePath(QStringLiteral("changing"));
    QVERIFY(QDir().mkpath(changingSource));
    writeFile(QDir(changingSource).filePath(QStringLiteral("payload.bin")),
              QByteArray(8 * 1024 * 1024, 'x'));
    bool lateCreated = false;
    const TransferResult changing = runFileTransfer(
        {changingSource}, destinationRoot.path(), true, cancelled,
        [&](const TransferUpdate &update) {
            if (!lateCreated && update.completedWork > 0) {
                writeFile(QDir(changingSource).filePath(QStringLiteral("late.txt")), "late");
                lateCreated = true;
            }
        });
    QVERIFY(lateCreated);
    QVERIFY(!changing.success);
    QCOMPARE(readFile(QDir(changingSource).filePath(QStringLiteral("late.txt"))),
             QByteArray("late"));
    QCOMPARE(readFile(QDir(changingSource).filePath(QStringLiteral("payload.bin"))).size(),
             8 * 1024 * 1024);

    const QString metadataSource = sourceRoot.filePath(QStringLiteral("metadata"));
    const QString metadataFile = QDir(metadataSource).filePath(QStringLiteral("item.txt"));
    QVERIFY(QDir().mkdir(metadataSource));
    writeFile(metadataFile, "metadata");
    QCOMPARE(::chmod(QFile::encodeName(metadataSource).constData(), 02750), 0);
    QCOMPARE(::chmod(QFile::encodeName(metadataFile).constData(), 0751), 0);
    const QByteArray attributeName("user.shibui-test");
    const QByteArray attributeValue("preserved");
    const bool extendedAttributesSupported =
        ::setxattr(QFile::encodeName(metadataFile).constData(), attributeName.constData(),
                   attributeValue.constData(), attributeValue.size(), 0) == 0;
    const struct timespec metadataTimes[] = {{1600000000, 123456789},
                                             {1600000010, 987654321}};
    QCOMPARE(::utimensat(AT_FDCWD, QFile::encodeName(metadataFile).constData(),
                         metadataTimes, 0), 0);
    QCOMPARE(::utimensat(AT_FDCWD, QFile::encodeName(metadataSource).constData(),
                         metadataTimes, 0), 0);

    const TransferResult metadataMove = runFileTransfer(
        {metadataSource}, destinationRoot.path(), true, cancelled, progress);
    QVERIFY2(metadataMove.success, qPrintable(metadataMove.message));
    const QString metadataTarget = destinationRoot.filePath(QStringLiteral("metadata"));
    const QString metadataTargetFile = QDir(metadataTarget).filePath(QStringLiteral("item.txt"));
    struct stat targetDirectoryStatus {};
    struct stat targetFileStatus {};
    QCOMPARE(::stat(QFile::encodeName(metadataTarget).constData(), &targetDirectoryStatus), 0);
    QCOMPARE(::stat(QFile::encodeName(metadataTargetFile).constData(), &targetFileStatus), 0);
    QCOMPARE(targetDirectoryStatus.st_mode & 07777, mode_t(02750));
    QCOMPARE(targetFileStatus.st_mode & 07777, mode_t(0751));
    QCOMPARE(targetDirectoryStatus.st_mtim.tv_sec, metadataTimes[1].tv_sec);
    QCOMPARE(targetDirectoryStatus.st_mtim.tv_nsec, metadataTimes[1].tv_nsec);
    QCOMPARE(targetFileStatus.st_mtim.tv_sec, metadataTimes[1].tv_sec);
    QCOMPARE(targetFileStatus.st_mtim.tv_nsec, metadataTimes[1].tv_nsec);
    if (extendedAttributesSupported) {
        QByteArray copiedValue(attributeValue.size(), Qt::Uninitialized);
        QCOMPARE(::getxattr(QFile::encodeName(metadataTargetFile).constData(),
                            attributeName.constData(), copiedValue.data(), copiedValue.size()),
                 ssize_t(attributeValue.size()));
        QCOMPARE(copiedValue, attributeValue);
    }
}

void CoreTest::movesItemsToTrashAndReportsPartialFailures()
{
    const QString source = QDir(m_testDataDirectory->path()).filePath(QStringLiteral("trash-source"));
    QVERIFY(QDir().mkpath(QDir(source).filePath(QStringLiteral("folder"))));
    const QString filePath = QDir(source).filePath(QStringLiteral("item.txt"));
    const QString folderPath = QDir(source).filePath(QStringLiteral("folder"));
    writeFile(filePath, "trash me");
    writeFile(QDir(folderPath).filePath(QStringLiteral("nested.txt")), "nested");

    FileSystemModel model;
    QVERIFY(model.navigateTo(source));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 2, 3000);
    QSignalSpy finished(&model, &FileSystemModel::trashFinished);

    QVERIFY(model.startTrash({filePath, folderPath}));
    QTRY_VERIFY_WITH_TIMEOUT(!model.trashActive(), 5000);
    QCOMPARE(finished.size(), 1);
    const QList<QVariant> successful = finished.takeFirst();
    QVERIFY(successful.at(0).toBool());
    QCOMPARE(successful.at(3).toStringList().size(), 2);
    QCOMPARE(successful.at(4).toStringList().size(), 2);
    QVERIFY(!QFileInfo::exists(filePath));
    QVERIFY(!QFileInfo::exists(folderPath));
    for (const QString &trashPath : successful.at(4).toStringList())
        QVERIFY(QFileInfo(trashPath).exists());
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 0, 3000);

    QVERIFY(model.navigateToTrash());
    QVERIFY(model.trashView());
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 2, 3000);
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.data(model.index(row), FileSystemModel::NameRole).toString()
                    == QStringLiteral("item.txt")) {
                return model.data(model.index(row), FileSystemModel::TypeTextRole).toString()
                    == source;
            }
        }
        return false;
    }(), 3000);

    QString trashedFilePath;
    QString trashedFolderPath;
    for (const QString &trashPath : successful.at(4).toStringList()) {
        if (QFileInfo(trashPath).isDir())
            trashedFolderPath = trashPath;
        else
            trashedFilePath = trashPath;
    }
    QVERIFY(!trashedFilePath.isEmpty());
    QVERIFY(!trashedFolderPath.isEmpty());

    QSignalSpy restored(&model, &FileSystemModel::transferFinished);
    QVERIFY(model.startRestore({trashedFilePath}));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QCOMPARE(restored.size(), 1);
    const QList<QVariant> restoredFile = restored.takeFirst();
    QVERIFY(restoredFile.at(0).toBool());
    QCOMPARE(restoredFile.at(3).toStringList(), QStringList{trashedFilePath});
    QCOMPARE(restoredFile.at(4).toStringList(), QStringList{filePath});
    QCOMPARE(readFile(filePath), QByteArray("trash me"));
    QVERIFY(!QFileInfo::exists(trashedFilePath));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 3000);

    QVERIFY(QDir().mkpath(folderPath));
    writeFile(QDir(folderPath).filePath(QStringLiteral("existing.txt")), "existing");
    QVERIFY(model.startRestore({trashedFolderPath}));
    QTRY_VERIFY_WITH_TIMEOUT(model.transferConflictActive(), 3000);
    QCOMPARE(model.transferConflictTarget(), folderPath);
    QVERIFY(model.resolveTransferConflict(QStringLiteral("rename"),
                                          QStringLiteral("restored-folder"), false));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QCOMPARE(restored.size(), 1);
    QVERIFY(restored.takeFirst().at(0).toBool());
    QCOMPARE(readFile(QDir(folderPath).filePath(QStringLiteral("existing.txt"))),
             QByteArray("existing"));
    QCOMPARE(readFile(QDir(source).filePath(QStringLiteral("restored-folder/nested.txt"))),
             QByteArray("nested"));
    QVERIFY(!QFileInfo::exists(trashedFolderPath));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 0, 3000);

    QVERIFY(model.goParent());
    QVERIFY(!model.trashView());
    QCOMPARE(model.currentPath(), source);

    const QString remainingPath = QDir(source).filePath(QStringLiteral("remaining.txt"));
    const QString missingPath = QDir(source).filePath(QStringLiteral("missing.txt"));
    writeFile(remainingPath, "also trash me");
    QVERIFY(model.startTrash({remainingPath, missingPath}));
    QTRY_VERIFY_WITH_TIMEOUT(!model.trashActive(), 5000);
    const QList<QVariant> partial = finished.takeFirst();
    QVERIFY(!partial.at(0).toBool());
    QVERIFY(!partial.at(1).toBool());
    QCOMPARE(partial.at(3).toStringList(), QStringList{remainingPath});
    QCOMPARE(partial.at(5).toStringList(), QStringList{missingPath});
    QVERIFY(partial.at(2).toString().contains(QStringLiteral("Moved 1 of 2")));
    QVERIFY(!QFileInfo::exists(remainingPath));

    QVERIFY(model.navigateToTrash());
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 3000);
    QVERIFY(model.startEmptyTrash());
    QTRY_VERIFY_WITH_TIMEOUT(!model.trashActive(), 5000);
    const QList<QVariant> emptied = finished.takeFirst();
    QVERIFY(emptied.at(0).toBool());
    QVERIFY(emptied.at(2).toString().contains(QStringLiteral("Emptied Trash")));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 0, 3000);
    const QString trashRoot = QDir(m_testDataDirectory->path()).filePath(QStringLiteral("Trash"));
    QCOMPARE(QDir(QDir(trashRoot).filePath(QStringLiteral("files")))
                 .entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot),
             QStringList());
    QCOMPARE(QDir(QDir(trashRoot).filePath(QStringLiteral("info")))
                 .entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot),
             QStringList());
}

void CoreTest::aggregatesTrashLocations()
{
    QTemporaryDir extraTrash;
    QVERIFY(extraTrash.isValid());
    const QString filesPath = extraTrash.filePath(QStringLiteral("files"));
    const QString infoPath = extraTrash.filePath(QStringLiteral("info"));
    QVERIFY(QDir().mkpath(filesPath));
    QVERIFY(QDir().mkpath(infoPath));
    const QString trashedPath = QDir(filesPath).filePath(QStringLiteral("external-item"));
    writeFile(trashedPath, "external trash");
    writeFile(QDir(infoPath).filePath(QStringLiteral("external-item.trashinfo")),
              "[Trash Info]\nPath=/media/example/original.txt\n"
              "DeletionDate=2026-08-27T10:00:00\n");
    qputenv("SHIBUI_ADDITIONAL_TRASH_ROOTS", extraTrash.path().toUtf8());

    FileSystemModel model;
    QVERIFY(model.navigateToTrash());
    QTRY_VERIFY_WITH_TIMEOUT(model.indexOfPath(trashedPath) >= 0, 3000);
    const int row = model.indexOfPath(trashedPath);
    QTRY_COMPARE_WITH_TIMEOUT(
        model.data(model.index(row), FileSystemModel::NameRole).toString(),
        QStringLiteral("original.txt"), 3000);
    QVERIFY(model.startEmptyTrash());
    QTRY_VERIFY_WITH_TIMEOUT(!model.trashActive(), 5000);
    QVERIFY(!QFileInfo::exists(trashedPath));

    qunsetenv("SHIBUI_ADDITIONAL_TRASH_ROOTS");
}

void CoreTest::undoesRecentMutationsSafely()
{
    QTemporaryDir directory(QDir(m_testDataDirectory->path())
                                 .filePath(QStringLiteral("undo-XXXXXX")));
    QVERIFY(directory.isValid());
    const QString source = directory.filePath(QStringLiteral("source"));
    const QString destination = directory.filePath(QStringLiteral("destination"));
    QVERIFY(QDir().mkpath(source));
    QVERIFY(QDir().mkpath(destination));

    FileSystemModel model;
    QVERIFY(model.navigateTo(source));

    const QString guardedFolder = model.createDirectory(QStringLiteral("guarded"));
    QVERIFY(!guardedFolder.isEmpty());
    writeFile(QDir(guardedFolder).filePath(QStringLiteral("external.txt")), "external");
    QVERIFY(!model.undoLast());
    QVERIFY(model.errorMessage().contains(QStringLiteral("no longer empty")));
    QVERIFY(QFile::remove(QDir(guardedFolder).filePath(QStringLiteral("external.txt"))));
    QVERIFY(model.undoLast());
    QVERIFY(!QFileInfo::exists(guardedFolder));
    QVERIFY(!model.canUndo());

    const QString created = model.createDirectory(QStringLiteral("created"));
    const QString renamed = model.renamePath(created, QStringLiteral("renamed"));
    QCOMPARE(renamed, QDir(source).filePath(QStringLiteral("renamed")));
    QVERIFY(model.undoLast());
    QVERIFY(QFileInfo(created).isDir());
    QVERIFY(!QFileInfo::exists(renamed));
    QVERIFY(model.undoLast());
    QVERIFY(!QFileInfo::exists(created));

    const QString moveSource = QDir(source).filePath(QStringLiteral("move.txt"));
    const QString moveDestination = QDir(destination).filePath(QStringLiteral("move.txt"));
    writeFile(moveSource, "move");
    QVERIFY(model.startTransfer({moveSource}, destination, true));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QVERIFY(QFileInfo::exists(moveDestination));
    QVERIFY(model.canUndo());
    QVERIFY(model.undoLast());
    QTRY_VERIFY_WITH_TIMEOUT(!model.undoActive(), 5000);
    QCOMPARE(readFile(moveSource), QByteArray("move"));
    QVERIFY(!QFileInfo::exists(moveDestination));

    const QString replacingSource = QDir(source).filePath(QStringLiteral("same.txt"));
    const QString replacingTarget = QDir(destination).filePath(QStringLiteral("same.txt"));
    writeFile(replacingSource, "new");
    writeFile(replacingTarget, "old");
    QVERIFY(model.startTransfer({replacingSource}, destination, true));
    QTRY_VERIFY_WITH_TIMEOUT(model.transferConflictActive(), 3000);
    QVERIFY(model.resolveTransferConflict(QStringLiteral("replace"), {}, false));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QCOMPARE(readFile(replacingTarget), QByteArray("new"));
    QVERIFY(model.undoLast());
    QTRY_VERIFY_WITH_TIMEOUT(!model.undoActive(), 5000);
    QCOMPARE(readFile(replacingSource), QByteArray("new"));
    QCOMPARE(readFile(replacingTarget), QByteArray("old"));

    const QString trashPath = QDir(source).filePath(QStringLiteral("trash.txt"));
    writeFile(trashPath, "trash");
    QVERIFY(model.startTrash({trashPath}));
    QTRY_VERIFY_WITH_TIMEOUT(!model.trashActive(), 5000);
    QVERIFY(!QFileInfo::exists(trashPath));
    QVERIFY(model.undoLast());
    QTRY_VERIFY_WITH_TIMEOUT(!model.undoActive(), 5000);
    QCOMPARE(readFile(trashPath), QByteArray("trash"));
    QVERIFY(!model.canUndo());

    const QString original = QDir(source).filePath(QStringLiteral("original.txt"));
    const QString changed = QDir(source).filePath(QStringLiteral("changed.txt"));
    writeFile(original, "renamed data");
    QCOMPARE(model.renamePath(original, QStringLiteral("changed.txt")), changed);
    writeFile(original, "external replacement");
    QVERIFY(!model.undoLast());
    QVERIFY(model.errorMessage().contains(QStringLiteral("changed externally")));
    QCOMPARE(readFile(original), QByteArray("external replacement"));
    QCOMPARE(readFile(changed), QByteArray("renamed data"));
    QVERIFY(model.canUndo());
}

void CoreTest::interoperatesWithDesktopClipboard()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("first file.txt"));
    const QString secondPath = directory.filePath(QStringLiteral("second.txt"));
    writeFile(firstPath, "first");
    writeFile(secondPath, "second");

    FileSystemModel model;
    model.setFileClipboard({firstPath, secondPath}, false);
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    QVERIFY(mimeData);
    QCOMPARE(mimeData->urls(), QList<QUrl>({QUrl::fromLocalFile(firstPath),
                                           QUrl::fromLocalFile(secondPath)}));
    QVERIFY(mimeData->hasFormat(QStringLiteral("x-special/gnome-copied-files")));
    QVERIFY(mimeData->data(QStringLiteral("x-special/gnome-copied-files"))
                .startsWith("copy\n"));
    QCOMPARE(model.fileClipboardPaths(), QStringList({firstPath, secondPath}));
    QVERIFY(!model.fileClipboardMove());

    auto *externalCut = new QMimeData;
    externalCut->setUrls({QUrl::fromLocalFile(secondPath)});
    externalCut->setData(QStringLiteral("application/x-kde-cutselection"), "1");
    QGuiApplication::clipboard()->setMimeData(externalCut);
    QCOMPARE(model.fileClipboardPaths(), QStringList{secondPath});
    QVERIFY(model.fileClipboardMove());

    auto *gnomeCut = new QMimeData;
    const QByteArray encoded = QByteArrayLiteral("cut\n")
        + QUrl::fromLocalFile(firstPath).toEncoded();
    gnomeCut->setData(QStringLiteral("x-special/gnome-copied-files"), encoded);
    QGuiApplication::clipboard()->setMimeData(gnomeCut);
    QCOMPARE(model.fileClipboardPaths(), QStringList{firstPath});
    QVERIFY(model.fileClipboardMove());

    QGuiApplication::clipboard()->clear();
    QVERIFY(model.fileClipboardPaths().isEmpty());
    QVERIFY(model.copyPathsAsText({firstPath, secondPath}));
    QCOMPARE(QGuiApplication::clipboard()->text(), firstPath + QLatin1Char('\n') + secondPath);
}

void CoreTest::listsStandardPlacesAndMountedVolumes()
{
    PlacesModel model;
    QTRY_VERIFY_WITH_TIMEOUT(model.rowCount() >= 3, 3000);
    bool foundHome = false;
    bool foundTrash = false;
    bool foundFileSystem = false;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row);
        const QString path = model.data(index, PlacesModel::PathRole).toString();
        foundHome |= path == QDir::homePath();
        foundTrash |= model.data(index, PlacesModel::TrashRole).toBool();
        foundFileSystem |= path == QStringLiteral("/");
        if (path == QDir::homePath())
            QCOMPARE(model.data(index, PlacesModel::SectionRole).toString(),
                     QStringLiteral("PLACES"));
        if (path == QStringLiteral("/"))
            QCOMPARE(model.data(index, PlacesModel::SectionRole).toString(),
                     QStringLiteral("DEVICES"));
    }
    QVERIFY(foundHome);
    QVERIFY(foundTrash);
    QVERIFY(foundFileSystem);

    QTemporaryDir firstBookmark;
    QTemporaryDir secondBookmark;
    QVERIFY(firstBookmark.isValid());
    QVERIFY(secondBookmark.isValid());
    QVERIFY(model.addBookmark(firstBookmark.path(), QStringLiteral("First")));
    QVERIFY(model.addBookmark(secondBookmark.path(), QStringLiteral("Second")));
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        int bookmarks = 0;
        for (int row = 0; row < model.rowCount(); ++row)
            bookmarks += model.isBookmarkAt(row) ? 1 : 0;
        return bookmarks == 2;
    }(), 3000);

    int firstRow = -1;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.pathAt(row) == firstBookmark.path())
            firstRow = row;
    }
    QVERIFY(firstRow >= 0);
    QVERIFY(model.renameBookmark(firstRow, QStringLiteral("Renamed")));
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.pathAt(row) == firstBookmark.path())
                return model.labelAt(row) == QStringLiteral("Renamed");
        }
        return false;
    }(), 3000);

    firstRow = -1;
    int secondRow = -1;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.pathAt(row) == firstBookmark.path())
            firstRow = row;
        else if (model.pathAt(row) == secondBookmark.path())
            secondRow = row;
    }
    QVERIFY(firstRow >= 0);
    QVERIFY(secondRow > firstRow);
    QVERIFY(model.moveBookmark(firstRow, 1));
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        int first = -1;
        int second = -1;
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.pathAt(row) == firstBookmark.path())
                first = row;
            else if (model.pathAt(row) == secondBookmark.path())
                second = row;
        }
        return second >= 0 && first > second;
    }(), 3000);

    PlacesModel reloaded;
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        for (int row = 0; row < reloaded.rowCount(); ++row) {
            if (reloaded.pathAt(row) == firstBookmark.path())
                return reloaded.labelAt(row) == QStringLiteral("Renamed");
        }
        return false;
    }(), 3000);

    const QString networkUri = QStringLiteral("smb://files.example/team");
    QVERIFY(model.addNetworkBookmark(networkUri, QStringLiteral("Team files")));
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.networkUriAt(row) == networkUri) {
                return model.isNetworkAt(row) && model.isBookmarkAt(row)
                    && model.pathAt(row).isEmpty()
                    && model.labelAt(row) == QStringLiteral("Team files");
            }
        }
        return false;
    }(), 3000);
    QVERIFY(!model.addNetworkBookmark(networkUri, QStringLiteral("Duplicate")));

    for (int row = model.rowCount() - 1; row >= 0; --row) {
        if (model.isBookmarkAt(row))
            QVERIFY(model.removeBookmark(row));
    }
}

void CoreTest::recursivelyFindsAndFuzzyRanksPaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir(directory.path()).mkpath(QStringLiteral("deep/reports")));
    QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("empty-folder")));
    QVERIFY(QDir(directory.path()).mkpath(QStringLiteral(".hidden")));
    QVERIFY(QDir(directory.path()).mkpath(QStringLiteral("ignored")));
    writeFile(directory.filePath(QStringLiteral("deep/reports/QuarterlyReport.txt")), "report");
    writeFile(directory.filePath(QStringLiteral("deep/notes.md")), "notes");
    writeFile(directory.filePath(QStringLiteral(".hidden/secret.txt")), "secret");
    writeFile(directory.filePath(QStringLiteral("ignored/generated.txt")), "generated");
    writeFile(directory.filePath(QStringLiteral(".gitignore")), "ignored/\n");
    QFile oldReport(directory.filePath(QStringLiteral("deep/reports/QuarterlyReport.txt")));
    QVERIFY(oldReport.open(QIODevice::ReadWrite));
    QVERIFY(oldReport.setFileTime(QDateTime::currentDateTime().addDays(-100),
                                  QFileDevice::FileModificationTime));
    oldReport.close();

    SearchModel model;
    QVERIFY(model.start(directory.path(), false));
    QTRY_VERIFY_WITH_TIMEOUT(!model.scanning(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(model.rowCount() >= 4, 3000);

    auto containsSuffix = [&model](const QString &suffix) {
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.pathAt(row).endsWith(suffix))
                return true;
        }
        return false;
    };
    QVERIFY(containsSuffix(QStringLiteral("deep/reports/QuarterlyReport.txt")));
    QVERIFY(containsSuffix(QStringLiteral("empty-folder")));
    QVERIFY(!containsSuffix(QStringLiteral(".hidden/secret.txt")));
    QVERIFY(containsSuffix(QStringLiteral("ignored/generated.txt")));
    QVERIFY(model.data(model.index(0), SearchModel::IconSourceRole).toString()
                .startsWith(QStringLiteral("image://fileicon/")));

    model.setQuery(QStringLiteral("qreport"));
    QTRY_VERIFY_WITH_TIMEOUT(model.rowCount() > 0
                             && model.pathAt(0).endsWith(
                                 QStringLiteral("QuarterlyReport.txt")), 3000);

    model.setQuery(QStringLiteral("empty"));
    QTRY_VERIFY_WITH_TIMEOUT(model.rowCount() > 0
                             && model.pathAt(0).endsWith(
                                 QStringLiteral("empty-folder")), 3000);
    QVERIFY(model.isDirectoryAt(0));

    model.setQuery(QString());
    model.setTypeFilter(QStringLiteral("folders"));
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        if (model.rowCount() == 0)
            return false;
        for (int row = 0; row < model.rowCount(); ++row) {
            if (!model.isDirectoryAt(row))
                return false;
        }
        return true;
    }(), 3000);

    model.setTypeFilter(QStringLiteral("documents"));
    model.setModifiedWithinDays(7);
    model.setQuery(QStringLiteral("qreport"));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 0, 3000);
    model.setModifiedWithinDays(365);
    QTRY_VERIFY_WITH_TIMEOUT(model.rowCount() > 0
                             && model.pathAt(0).endsWith(
                                 QStringLiteral("QuarterlyReport.txt")), 3000);

    model.setQuery(QString());
    model.setTypeFilter(QStringLiteral("all"));
    model.setModifiedWithinDays(0);
    QVERIFY(model.start(directory.path(), true));
    QTRY_VERIFY_WITH_TIMEOUT(!model.scanning(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        return containsSuffix(QStringLiteral(".hidden/secret.txt"));
    }(), 3000);
    QVERIFY(containsSuffix(QStringLiteral("ignored/generated.txt")));
}

void CoreTest::previewsCommonLocalFilesAsynchronously()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString textPath = directory.filePath(QStringLiteral("readme.txt"));
    const QString binaryPath = directory.filePath(QStringLiteral("data.bin"));
    const QString imagePath = directory.filePath(QStringLiteral("photo.png"));
    writeFile(textPath, "hello from preview\n");
    writeFile(binaryPath, QByteArray("binary\0data", 11));
    writeFile(imagePath, QByteArrayLiteral("not-decoded-by-the-model"));

    PreviewModel model;
    QVERIFY(model.open(textPath));
    QVERIFY(model.active());
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QCOMPARE(model.kind(), QStringLiteral("text"));
    QVERIFY(model.text().contains(QStringLiteral("hello from preview")));

    QVERIFY(model.open(binaryPath));
    QCOMPARE(model.kind(), QStringLiteral("unsupported"));
    QVERIFY(model.open(imagePath));
    QCOMPARE(model.kind(), QStringLiteral("image"));
    QVERIFY(model.imageSource().startsWith(QStringLiteral("file:")));
    model.close();
    QVERIFY(!model.active());
}

void CoreTest::listsCompatibleApplications()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("document.txt"));
    writeFile(path, "open with\n");

    OpenWithModel model;
    QVERIFY(model.open(path));
    QVERIFY(model.active());
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QVERIFY2(model.rowCount() > 0, qPrintable(model.errorMessage()));
    QCOMPARE(model.data(model.index(0), OpenWithModel::NameRole).toString(),
             QStringLiteral("Shibui Test Viewer"));
    QCOMPARE(model.desktopIdAt(0), QStringLiteral("shibui-test.desktop"));
    model.close();
    QVERIFY(!model.active());
}

void CoreTest::reportsPropertiesAndCalculatesFolderSize()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeFile(directory.filePath(QStringLiteral("first.txt")), QByteArray(12, 'a'));
    writeFile(directory.filePath(QStringLiteral("second.txt")), QByteArray(20, 'b'));
    QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral(".hidden"))));
    QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("empty"))));
    writeFile(directory.filePath(QStringLiteral(".hidden/data.bin")), QByteArray(7, 'c'));

    PropertiesModel model;
    QVERIFY(model.open(directory.filePath(QStringLiteral("first.txt"))));
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QCOMPARE(model.name(), QStringLiteral("first.txt"));
    QCOMPARE(model.mimeType(), QStringLiteral("text/plain"));
    QVERIFY(model.permissions().size() == 9);
    QVERIFY(!model.filesystemFree().isEmpty());
    QVERIFY(model.permissionsEditable());
    const QFile::Permissions originalPermissions = QFileInfo(
        directory.filePath(QStringLiteral("first.txt"))).permissions();
    QVERIFY(model.togglePermissionBit(6));
    QVERIFY(QFileInfo(directory.filePath(QStringLiteral("first.txt"))).permissions()
            != originalPermissions);
    QVERIFY(model.togglePermissionBit(6));
    QCOMPARE(QFileInfo(directory.filePath(QStringLiteral("first.txt"))).permissions(),
             originalPermissions);

    QVERIFY(model.open(directory.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QVERIFY(model.directory());
    QVERIFY(model.calculateDirectorySize());
    QVERIFY(model.open(directory.filePath(QStringLiteral("first.txt"))));
    QVERIFY(!model.sizing());
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QVERIFY(model.open(directory.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QVERIFY(model.calculateDirectorySize());
    QTRY_VERIFY_WITH_TIMEOUT(!model.sizing(), 3000);
    QVERIFY(model.recursiveSize().contains(QStringLiteral("5 items")));
    model.close();
}

void CoreTest::validatesNetworkLocationsWithoutStoringCredentials()
{
    NetworkModel model;
    QVERIFY(!model.connectTo(QStringLiteral("https://example.com")));
    QVERIFY(model.errorMessage().contains(QStringLiteral("SMB")));
    QVERIFY(!model.connectTo(QStringLiteral("sftp:///missing-host")));
    QVERIFY(!model.connecting());
}

void CoreTest::matchesExactNetworkSharesAndMasksSplitPrompts()
{
    QTemporaryDir gvfs;
    QVERIFY(gvfs.isValid());
    const QString shareOne = gvfs.filePath(
        QStringLiteral("smb-share:server=files.example,share=one"));
    const QString shareTwo = gvfs.filePath(
        QStringLiteral("smb-share:server=files.example,share=two"));
    QVERIFY(QDir().mkpath(shareOne));
    QVERIFY(QDir().mkpath(shareTwo));
    qputenv("SHIBUI_GVFS_ROOT", gvfs.path().toUtf8());

    NetworkModel mounted;
    QSignalSpy connected(&mounted, &NetworkModel::connected);
    QVERIFY(mounted.connectTo(QStringLiteral("smb://files.example/two")));
    QCOMPARE(connected.size(), 1);
    QCOMPARE(connected.constFirst().at(0).toString(), shareTwo);

    QTemporaryDir commands;
    QVERIFY(commands.isValid());
    const QString gioPath = commands.filePath(QStringLiteral("gio"));
    writeFile(gioPath, "#!/bin/sh\nprintf Pass\nsleep 0.05\nprintf word:\nread answer\nexit 1\n");
    QVERIFY(QFile::setPermissions(gioPath, QFile::ReadOwner | QFile::WriteOwner
                                              | QFile::ExeOwner));
    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", commands.path().toUtf8() + ':' + oldPath);

    NetworkModel prompted;
    QVERIFY(prompted.connectTo(QStringLiteral("smb://prompt.example/secret")));
    QTRY_VERIFY_WITH_TIMEOUT(prompted.promptActive(), 3000);
    QVERIFY(prompted.promptSecret());
    prompted.submitResponse(QStringLiteral("not-a-real-password"));
    QTRY_VERIFY_WITH_TIMEOUT(!prompted.connecting(), 3000);

    qputenv("PATH", oldPath);
    qunsetenv("SHIBUI_GVFS_ROOT");
}

void CoreTest::subprocessStartFailuresClearActiveState()
{
    QTemporaryDir directory;
    QTemporaryDir commands;
    QVERIFY(directory.isValid());
    QVERIFY(commands.isValid());
    const QStringList names = {
        QStringLiteral("fd"), QStringLiteral("gio"), QStringLiteral("pdftoppm"),
        QStringLiteral("bsdtar"),
    };
    for (const QString &name : names) {
        const QString path = commands.filePath(name);
        writeFile(path, "#!/definitely/missing/interpreter\n");
        QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner
                                              | QFile::ExeOwner));
    }
    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", commands.path().toUtf8());

    SearchModel search;
    QVERIFY(search.start(directory.path(), false));
    QTRY_VERIFY_WITH_TIMEOUT(!search.scanning(), 3000);
    QVERIFY(!search.errorMessage().isEmpty());

    const QString textPath = directory.filePath(QStringLiteral("document.txt"));
    const QString pdfPath = directory.filePath(QStringLiteral("document.pdf"));
    writeFile(textPath, "text");
    writeFile(pdfPath, "%PDF-1.4\n");
    OpenWithModel openWith;
    QVERIFY(openWith.open(textPath));
    QTRY_VERIFY_WITH_TIMEOUT(!openWith.loading(), 3000);
    QVERIFY(!openWith.errorMessage().isEmpty());

    PreviewModel preview;
    QVERIFY(preview.open(pdfPath));
    QTRY_VERIFY_WITH_TIMEOUT(!preview.loading(), 3000);
    QVERIFY(!preview.errorMessage().isEmpty());

    ArchiveModel archive;
    QVERIFY(archive.createArchive({textPath}, directory.path(), QStringLiteral("broken.tar")));
    QTRY_VERIFY_WITH_TIMEOUT(!archive.active(), 3000);
    QVERIFY(!archive.errorMessage().isEmpty());
    QVERIFY(!QFileInfo::exists(directory.filePath(QStringLiteral("broken.tar"))));

    qputenv("PATH", oldPath);
}

void CoreTest::createsAndExtractsCommonArchives()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir(directory.path()).mkpath(QStringLiteral("folder")));
    const QString textPath = directory.filePath(QStringLiteral("alpha.txt"));
    const QString folderPath = directory.filePath(QStringLiteral("folder"));
    writeFile(textPath, "alpha");
    writeFile(QDir(folderPath).filePath(QStringLiteral("beta.txt")), "beta");

    ArchiveModel model;
    QSignalSpy finished(&model, &ArchiveModel::finished);
    QVERIFY(model.createArchive({textPath, folderPath}, directory.path(),
                                QStringLiteral("bundle.zip")));
    QVERIFY(model.active());
    QTRY_VERIFY_WITH_TIMEOUT(!model.active(), 5000);
    QCOMPARE(finished.size(), 1);
    QVERIFY(finished.constFirst().at(0).toBool());
    const QString zipPath = directory.filePath(QStringLiteral("bundle.zip"));
    QVERIFY(QFileInfo(zipPath).isFile());
    QVERIFY(model.supportsArchive(zipPath));

    QVERIFY(model.extractArchive(zipPath, directory.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!model.active(), 5000);
    QCOMPARE(finished.size(), 2);
    QVERIFY(finished.constLast().at(0).toBool());
    QCOMPARE(readFile(directory.filePath(QStringLiteral("bundle/alpha.txt"))),
             QByteArray("alpha"));
    QCOMPARE(readFile(directory.filePath(QStringLiteral("bundle/folder/beta.txt"))),
             QByteArray("beta"));

    QVERIFY(model.createArchive({textPath}, directory.path(),
                                QStringLiteral("single.tar.gz")));
    QTRY_VERIFY_WITH_TIMEOUT(!model.active(), 5000);
    QVERIFY(finished.constLast().at(0).toBool());
    QVERIFY(QFileInfo(directory.filePath(QStringLiteral("single.tar.gz"))).isFile());

    const QString largePath = directory.filePath(QStringLiteral("large.bin"));
    QFile large(largePath);
    QVERIFY(large.open(QIODevice::WriteOnly));
    QVERIFY(large.resize(64 * 1024 * 1024));
    large.close();
    const QString racedPath = directory.filePath(QStringLiteral("raced.tar"));
    QVERIFY(model.createArchive({largePath}, directory.path(), QStringLiteral("raced.tar")));
    writeFile(racedPath, "external");
    QTRY_VERIFY_WITH_TIMEOUT(!model.active(), 10000);
    QVERIFY(!finished.constLast().at(0).toBool());
    QCOMPARE(readFile(racedPath), QByteArray("external"));
}

void CoreTest::readsDesktopRecentFilesWithoutKeepingAnotherHistory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString olderPath = directory.filePath(QStringLiteral("older.txt"));
    const QString newerPath = directory.filePath(QStringLiteral("newer.txt"));
    writeFile(olderPath, "older");
    writeFile(newerPath, "newer");
    const QString missingPath = directory.filePath(QStringLiteral("missing.txt"));

    const QString xbelPath = QDir(QStandardPaths::writableLocation(
                                      QStandardPaths::GenericDataLocation))
                                 .filePath(QStringLiteral("recently-used.xbel"));
    const QByteArray xbel = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<xbel>"
        "<bookmark href=\"%1\" modified=\"2026-01-01T10:00:00Z\"/>"
        "<bookmark href=\"%2\" modified=\"2026-08-01T10:00:00Z\"/>"
        "<bookmark href=\"%3\" modified=\"2026-08-02T10:00:00Z\"/>"
        "</xbel>")
        .arg(QUrl::fromLocalFile(olderPath).toString(),
             QUrl::fromLocalFile(newerPath).toString(),
             QUrl::fromLocalFile(missingPath).toString()).toUtf8();
    writeFile(xbelPath, xbel);

    RecentModel model;
    model.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.pathAt(0), newerPath);
    QCOMPARE(model.pathAt(1), olderPath);
    QCOMPARE(model.data(model.index(0), RecentModel::RelativePathRole).toString(),
             directory.path());
    QVERIFY(model.data(model.index(0), RecentModel::IconSourceRole).toString()
                .startsWith(QStringLiteral("image://fileicon/")));
    QVERIFY(model.errorMessage().isEmpty());
}

void CoreTest::previewsAndAppliesBulkRenames()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString apple = directory.filePath(QStringLiteral("apple.txt"));
    const QString banana = directory.filePath(QStringLiteral("banana.txt"));
    writeFile(apple, "apple");
    writeFile(banana, "banana");

    BulkRenameModel model;
    QVERIFY(model.begin({banana, apple}));
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(!model.canApply());
    model.setFindText(QStringLiteral(".txt"));
    model.setReplacementText(QStringLiteral(".md"));
    QVERIFY(model.canApply());
    QCOMPARE(model.data(model.index(0), BulkRenameModel::ProposedNameRole).toString(),
             QStringLiteral("apple.md"));

    QSignalSpy finished(&model, &BulkRenameModel::finished);
    QVERIFY(model.apply());
    QTRY_VERIFY_WITH_TIMEOUT(!model.applying(), 3000);
    QCOMPARE(finished.size(), 1);
    QVERIFY(finished.constFirst().at(0).toBool());
    const QString appleMarkdown = directory.filePath(QStringLiteral("apple.md"));
    const QString bananaMarkdown = directory.filePath(QStringLiteral("banana.md"));
    QCOMPARE(readFile(appleMarkdown), QByteArray("apple"));
    QCOMPARE(readFile(bananaMarkdown), QByteArray("banana"));

    QVERIFY(model.begin({appleMarkdown, bananaMarkdown}));
    model.setNumbering(true);
    model.setReplacementText(QStringLiteral("Photo"));
    QVERIFY(model.canApply());
    QCOMPARE(model.data(model.index(0), BulkRenameModel::ProposedNameRole).toString(),
             QStringLiteral("Photo 01.md"));
    QVERIFY(model.apply());
    QTRY_VERIFY_WITH_TIMEOUT(!model.applying(), 3000);
    QVERIFY(QFileInfo(directory.filePath(QStringLiteral("Photo 01.md"))).isFile());
    QVERIFY(QFileInfo(directory.filePath(QStringLiteral("Photo 02.md"))).isFile());

    const QString first = directory.filePath(QStringLiteral("first.txt"));
    const QString second = directory.filePath(QStringLiteral("second.txt"));
    writeFile(first, "first");
    writeFile(second, "second");
    writeFile(directory.filePath(QStringLiteral("first.md")), "occupied");
    QVERIFY(model.begin({first, second}));
    model.setFindText(QStringLiteral(".txt"));
    model.setReplacementText(QStringLiteral(".md"));
    QVERIFY(!model.canApply());
    QVERIFY(!model.data(model.index(0), BulkRenameModel::ValidRole).toBool());
}

void CoreTest::createsDocumentsFromTheStandardTemplatesFolder()
{
    QTemporaryDir templates;
    QTemporaryDir destination;
    QVERIFY(templates.isValid());
    QVERIFY(destination.isValid());
    QVERIFY(QDir(templates.path()).mkdir(QStringLiteral("notes")));
    writeFile(templates.filePath(QStringLiteral("Blank.txt")), "template text");
    writeFile(templates.filePath(QStringLiteral("notes/Note.md")), "# Note\n");

    TemplateModel model(templates.path());
    QVERIFY(model.begin(destination.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.suggestedNameAt(0), QStringLiteral("Blank.txt"));

    QSignalSpy finished(&model, &TemplateModel::finished);
    QVERIFY(model.createFrom(0, QStringLiteral("Draft.txt")));
    QTRY_VERIFY_WITH_TIMEOUT(!model.copying(), 3000);
    QCOMPARE(finished.size(), 1);
    QVERIFY(finished.constFirst().at(0).toBool());
    const QString outputPath = destination.filePath(QStringLiteral("Draft.txt"));
    QCOMPARE(readFile(outputPath), QByteArray("template text"));
    QVERIFY(QFileInfo(outputPath).permission(QFileDevice::WriteOwner));

    QVERIFY(model.begin(destination.path()));
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QVERIFY(!model.createFrom(0, QStringLiteral("Draft.txt")));
    QVERIFY(model.errorMessage().contains(QStringLiteral("already exists")));
}

void CoreTest::createsAnUndoableFolderFromTheSelection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString first = directory.filePath(QStringLiteral("first.txt"));
    const QString second = directory.filePath(QStringLiteral("second.txt"));
    writeFile(first, "first");
    writeFile(second, "second");

    FileSystemModel model;
    QVERIFY(model.navigateTo(directory.path()));
    QVERIFY(model.startFolderWithSelection(QStringLiteral("Grouped"), {first, second}));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    const QString grouped = directory.filePath(QStringLiteral("Grouped"));
    QVERIFY(QFileInfo(QDir(grouped).filePath(QStringLiteral("first.txt"))).isFile());
    QVERIFY(QFileInfo(QDir(grouped).filePath(QStringLiteral("second.txt"))).isFile());
    QVERIFY(model.undoDescription().contains(QStringLiteral("Grouped")));

    QVERIFY(model.undoLast());
    QTRY_VERIFY_WITH_TIMEOUT(!model.undoActive(), 5000);
    QCOMPARE(readFile(first), QByteArray("first"));
    QCOMPARE(readFile(second), QByteArray("second"));
    QVERIFY(!QFileInfo::exists(grouped));
    QVERIFY(!model.canUndo());

    QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("Occupied")));
    QVERIFY(!model.startFolderWithSelection(QStringLiteral("Occupied"), {first, second}));
    QVERIFY(QFileInfo(first).isFile());
    QVERIFY(QFileInfo(second).isFile());

    QVERIFY(model.startFolderWithSelection(QStringLiteral("Guarded"), {first}));
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    const QString guarded = directory.filePath(QStringLiteral("Guarded"));
    writeFile(QDir(guarded).filePath(QStringLiteral("external.txt")), "external");
    QVERIFY(model.undoLast());
    QTRY_VERIFY_WITH_TIMEOUT(!model.undoActive(), 5000);
    QVERIFY(model.errorMessage().contains(QStringLiteral("added later")));
    QVERIFY(!QFileInfo::exists(first));
    QVERIFY(QFileInfo(QDir(guarded).filePath(QStringLiteral("first.txt"))).isFile());
    QVERIFY(QFileInfo(QDir(guarded).filePath(QStringLiteral("external.txt"))).isFile());
    QVERIFY(model.canUndo());
}

void CoreTest::keyboardContractDrivesTheRealInterface()
{
    QTemporaryDir directory(QDir(m_testDataDirectory->path())
                                 .filePath(QStringLiteral("keyboard-XXXXXX")));
    QVERIFY(directory.isValid());
    QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("folder")));
    writeFile(directory.filePath(QStringLiteral("alpha.txt")), "alpha");
    writeFile(directory.filePath(QStringLiteral("omega.txt")), "omega");
    writeFile(directory.filePath(QStringLiteral(".secret")), "hidden");

    QTemporaryDir themeDirectory;
    QVERIFY(themeDirectory.isValid());
    writeFile(themeDirectory.filePath(QStringLiteral("colors.toml")), R"(
background = "#101820"
dark_background = "#0a1016"
lighter_background = "#1a2832"
foreground = "#d8e0e8"
muted = "#708090"
accent = "#55aa88"
selection = "#284438"
red = "#ee5566"
)");
    writeFile(themeDirectory.filePath(QStringLiteral("shell.toml")), R"(
[font]
base-size = 12
[spacing]
scale = 1
scale-with-font = true
)");
    writeFile(themeDirectory.filePath(QStringLiteral("icons.theme")), "hicolor\n");

    ThemeManager theme(themeDirectory.path());
    FileSystemModel model;
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
    QVERIFY(model.navigateTo(directory.path()));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 3, 3000);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("fileicon"), new FileIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("fileModel"), &model);
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
    engine.rootContext()->setContextProperty(QStringLiteral("initialSelectionPath"), QString());
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QCOMPARE(window->minimumWidth(), 844);
    window->requestActivate();
    QTRY_COMPARE_WITH_TIMEOUT(window->property("currentItemIndex").toInt(), 0, 2000);

    auto clickRow = [window](int row, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        const int rowHeight = window->property("rowHeight").toInt();
        const int y = window->property("topHeight").toInt()
            + window->property("columnHeaderHeight").toInt() + row * rowHeight + rowHeight / 2;
        const int x = window->property("sidebarWidth").toInt() + 24;
        QTest::mouseClick(window, Qt::LeftButton, modifiers, QPoint(x, y));
    };
    clickRow(0, Qt::ControlModifier);
    QCOMPARE(window->property("selectedItemCount").toInt(), 1);
    clickRow(2, Qt::ShiftModifier);
    QCOMPARE(window->property("selectedItemCount").toInt(), 3);
    clickRow(1);
    QCOMPARE(window->property("selectedItemCount").toInt(), 0);
    clickRow(0, Qt::ControlModifier);
    clickRow(2, Qt::ControlModifier);
    QCOMPARE(window->property("selectedItemCount").toInt(), 2);
    QTest::keyClick(window, Qt::Key_R, Qt::ShiftModifier);
    QVERIFY(window->property("bulkRenameVisible").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QVERIFY(!window->property("bulkRenameVisible").toBool());
    QTest::keyClick(window, Qt::Key_N, Qt::ShiftModifier);
    QVERIFY(window->property("templateChooserVisible").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QVERIFY(!window->property("templateChooserVisible").toBool());
    QTest::keyClick(window, Qt::Key_G);
    QTest::keyClick(window, Qt::Key_N);
    QTRY_VERIFY_WITH_TIMEOUT(window->property("operationPromptFocused").toBool(), 1000);
    QTest::keyClick(window, Qt::Key_Escape);
    QVERIFY(!window->property("operationPromptFocused").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QCOMPARE(window->property("selectedItemCount").toInt(), 0);
    QTest::keyClick(window, Qt::Key_Home);
    QCOMPARE(window->property("currentItemIndex").toInt(), 0);
    const int contextY = window->property("topHeight").toInt()
        + window->property("columnHeaderHeight").toInt()
        + window->property("rowHeight").toInt() * 3 / 2;
    const int contextX = window->property("sidebarWidth").toInt() + 24;
    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, QPoint(contextX, contextY));
    QTRY_VERIFY(window->property("contextMenuVisible").toBool());
    QCOMPARE(window->property("currentItemIndex").toInt(), 1);
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!window->property("contextMenuVisible").toBool());
    QTest::keyClick(window, Qt::Key_Home);
    QTRY_VERIFY_WITH_TIMEOUT(places.rowCount() >= 3, 3000);
    QVERIFY(window->property("placesSidebarVisible").toBool());
    QTest::keyClick(window, Qt::Key_B, Qt::ShiftModifier);
    QVERIFY(!window->property("placesSidebarVisible").toBool());
    QTest::keyClick(window, Qt::Key_B, Qt::ShiftModifier);
    QVERIFY(window->property("placesSidebarVisible").toBool());
    QTest::keyClick(window, Qt::Key_B);
    QVERIFY(window->property("placesModeActive").toBool());
    QCOMPARE(window->property("placesCurrentIndex").toInt(),
             window->property("activePlaceIndex").toInt());
    QTest::keyClick(window, Qt::Key_J);
    QTest::keyClick(window, Qt::Key_Escape);
    QVERIFY(!window->property("placesModeActive").toBool());

    QTest::keyClick(window, Qt::Key_J);
    QCOMPARE(window->property("currentItemIndex").toInt(), 1);
    QTest::keyClick(window, Qt::Key_K);
    QCOMPARE(window->property("currentItemIndex").toInt(), 0);

    QTest::keyClick(window, Qt::Key_I);
    QVERIFY(window->property("gridViewActive").toBool());
    QTest::keyClick(window, Qt::Key_J);
    QCOMPARE(window->property("currentItemIndex").toInt(), 1);
    QTest::keyClick(window, Qt::Key_1, Qt::ControlModifier);
    QVERIFY(!window->property("gridViewActive").toBool());
    QCOMPARE(window->property("currentItemIndex").toInt(), 1);
    QTest::keyClick(window, Qt::Key_2, Qt::ControlModifier);
    QVERIFY(window->property("gridViewActive").toBool());
    QTest::keyClick(window, Qt::Key_1, Qt::ControlModifier);
    QVERIFY(!window->property("gridViewActive").toBool());
    QTest::keyClick(window, Qt::Key_Home);

    const int selectionBeforePreview = window->property("selectedItemCount").toInt();
    QTest::keyClick(window, Qt::Key_Space);
    QVERIFY(window->property("previewVisible").toBool());
    QCOMPARE(window->property("selectedItemCount").toInt(), selectionBeforePreview);
    QTest::keyClick(window, Qt::Key_Space);
    QVERIFY(!window->property("previewVisible").toBool());
    QTest::keyClick(window, Qt::Key_X);
    QCOMPARE(window->property("selectedItemCount").toInt(), selectionBeforePreview + 1);
    QTest::keyClick(window, Qt::Key_X);
    QCOMPARE(window->property("selectedItemCount").toInt(), selectionBeforePreview);
    QTest::keyClick(window, Qt::Key_J);
    QTest::keyClick(window, Qt::Key_O);
    QTRY_VERIFY_WITH_TIMEOUT(window->property("openWithVisible").toBool(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(openWith.rowCount() > 0, 3000);
    QTest::keyClick(window, Qt::Key_Escape);
    QVERIFY(!window->property("openWithVisible").toBool());
    QTest::keyClick(window, Qt::Key_Home);

    QTest::keyClick(window, Qt::Key_Z);
    QVERIFY(window->property("propertiesVisible").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(!properties.loading(), 3000);
    QVERIFY(properties.directory());
    QTest::keyClick(window, Qt::Key_Escape);
    QVERIFY(!window->property("propertiesVisible").toBool());

    QTest::keyClick(window, Qt::Key_K, Qt::ControlModifier);
    QVERIFY(window->property("networkPromptFocused").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QVERIFY(!window->property("networkPromptFocused").toBool());

    QTest::keyClick(window, Qt::Key_G);
    QCOMPARE(window->property("pendingSequence").toString(), QStringLiteral("g"));
    QTest::keyClick(window, Qt::Key_G);
    QCOMPARE(window->property("pendingSequence").toString(), QString());
    QCOMPARE(window->property("currentItemIndex").toInt(), 0);

    QTest::keyClick(window, Qt::Key_G, Qt::ShiftModifier);
    QCOMPARE(window->property("currentItemIndex").toInt(), 2);
    QTest::keyClick(window, Qt::Key_Home);
    QCOMPARE(window->property("currentItemIndex").toInt(), 0);
    QTest::keyClick(window, Qt::Key_D, Qt::ControlModifier);
    QCOMPARE(window->property("currentItemIndex").toInt(), 2);
    QTest::keyClick(window, Qt::Key_U, Qt::ControlModifier);
    QCOMPARE(window->property("currentItemIndex").toInt(), 0);

    const int originalSort = model.sortField();
    QTest::keyClick(window, Qt::Key_S);
    QCOMPARE(model.sortField(), (originalSort + 1) % 4);
    model.setSort(FileSystemModel::NameSort, false);
    QTRY_COMPARE(window->property("currentItemIndex").toInt(), 0);

    QTest::keyClick(window, Qt::Key_L);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.filePath(QStringLiteral("folder")), 2000);
    QTest::keyClick(window, Qt::Key_H);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.path(), 2000);
    QVERIFY(window->property("historyBackAvailable").toBool());
    QTest::keyClick(window, Qt::Key_BracketLeft);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.filePath(QStringLiteral("folder")),
                              2000);
    QVERIFY(window->property("historyForwardAvailable").toBool());
    QTest::keyClick(window, Qt::Key_BracketRight);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.path(), 2000);
    QTRY_COMPARE(model.pathAt(window->property("currentItemIndex").toInt()),
                 directory.filePath(QStringLiteral("folder")));

    QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
    QTRY_VERIFY(window->property("locationPromptFocused").toBool());
    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    typeText(window, QStringLiteral("missing"));
    QTest::keyClick(window, Qt::Key_Return);
    QVERIFY(window->property("locationPromptFocused").toBool());
    QVERIFY(model.errorMessage().contains(QStringLiteral("does not exist")));
    QTest::keyClick(window, Qt::Key_Escape);

    QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    typeText(window, QStringLiteral("folder"));
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.filePath(QStringLiteral("folder")),
                              2000);
    QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    typeText(window, QStringLiteral(".."));
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.path(), 2000);

    QTest::keyClick(window, Qt::Key_M);
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        for (int row = 0; row < places.rowCount(); ++row) {
            if (places.isBookmarkAt(row) && places.pathAt(row) == directory.path())
                return true;
        }
        return false;
    }(), 3000);

    QTest::keyClick(window, Qt::Key_F);
    QTRY_VERIFY(window->property("finderPromptFocused").toBool());
    QVERIFY(window->property("finderModeActive").toBool());
    auto *finderHeader = window->findChild<QQuickItem *>(QStringLiteral("finderHeader"));
    auto *finderScopeRow = window->findChild<QQuickItem *>(QStringLiteral("finderScopeRow"));
    auto *finderFilters = window->findChild<QQuickItem *>(QStringLiteral("finderFilters"));
    QVERIFY(finderHeader);
    QVERIFY(finderScopeRow);
    QVERIFY(finderFilters);
    QVERIFY(finderFilters->y() >= finderScopeRow->y() + finderScopeRow->height());
    QVERIFY(finderHeader->height() >= finderFilters->y() + finderFilters->height());
    typeText(window, QStringLiteral("omega"));
    QTRY_COMPARE_WITH_TIMEOUT(window->property("finderPromptText").toString(),
                              QStringLiteral("omega"), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(window->property("finderResultCount").toInt() > 0, 3000);
    QCOMPARE(search.pathAt(window->property("finderCurrentIndex").toInt()),
             directory.filePath(QStringLiteral("omega.txt")));
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        QQuickItem *label = findQuickItem(window->contentItem(),
                                          QStringLiteral("finderResultName"));
        return label && label->property("text").toString().contains(
                            QStringLiteral("<font"));
    }(), 1000);
    QTest::keyClick(window, Qt::Key_Return, Qt::AltModifier);
    QTRY_VERIFY(window->property("propertiesVisible").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(!properties.loading(), 3000);
    QCOMPARE(properties.path(), directory.filePath(QStringLiteral("omega.txt")));
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!window->property("propertiesVisible").toBool());
    QTRY_VERIFY(window->property("finderPromptFocused").toBool());
    QTest::keyClick(window, Qt::Key_T, Qt::AltModifier);
    QTRY_COMPARE(search.typeFilter(), QStringLiteral("files"));
    QTest::keyClick(window, Qt::Key_D, Qt::AltModifier);
    QTRY_COMPARE(search.modifiedWithinDays(), 1);
    QTest::keyClick(window, Qt::Key_Return, Qt::ControlModifier);
    QVERIFY(!window->property("finderModeActive").toBool());
    QTRY_COMPARE(model.pathAt(window->property("currentItemIndex").toInt()),
                 directory.filePath(QStringLiteral("omega.txt")));

    QTest::keyClick(window, Qt::Key_Home);
    QTest::keyClick(window, Qt::Key_T);
    QTRY_COMPARE_WITH_TIMEOUT(window->property("tabCount").toInt(), 2, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.filePath(QStringLiteral("folder")),
                              2000);
    QTest::keyClick(window, Qt::Key_W, Qt::ControlModifier);
    QTRY_COMPARE(window->property("tabCount").toInt(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.path(), 2000);

    QTest::keyClick(window, Qt::Key_Home);
    clickRow(0);
    const int middleY = window->property("topHeight").toInt()
        + window->property("columnHeaderHeight").toInt()
        + window->property("rowHeight").toInt() / 2;
    QTest::mouseClick(window, Qt::MiddleButton, Qt::NoModifier,
                      QPoint(window->property("sidebarWidth").toInt() + 24, middleY));
    QTRY_COMPARE(window->property("tabCount").toInt(), 2);
    QCOMPARE(model.currentPath(), directory.path());
    QTest::keyClick(window, Qt::Key_G);
    QTest::keyClick(window, Qt::Key_T);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.filePath(QStringLiteral("folder")),
                              2000);
    QTest::keyClick(window, Qt::Key_W, Qt::ControlModifier);
    QTRY_COMPARE(window->property("tabCount").toInt(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.path(), 2000);

    const QString vanishingTabPath = directory.filePath(QStringLiteral("vanishing-tab"));
    QVERIFY(QDir().mkdir(vanishingTabPath));
    model.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(model.indexOfPath(vanishingTabPath) >= 0, 3000);
    QTest::keyClick(window, Qt::Key_Home);
    for (int step = 0; step < model.rowCount()
         && model.pathAt(window->property("currentItemIndex").toInt()) != vanishingTabPath;
         ++step) {
        QTest::keyClick(window, Qt::Key_J);
    }
    QCOMPARE(model.pathAt(window->property("currentItemIndex").toInt()), vanishingTabPath);
    QTest::keyClick(window, Qt::Key_T);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), vanishingTabPath, 2000);
    QCOMPARE(window->property("activeTabIndex").toInt(), 1);
    QTest::keyClick(window, Qt::Key_Tab, Qt::ControlModifier);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.path(), 2000);
    QCOMPARE(window->property("activeTabIndex").toInt(), 0);
    QVERIFY(QDir().rmdir(vanishingTabPath));
    QTest::keyClick(window, Qt::Key_Tab, Qt::ControlModifier);
    QTRY_COMPARE_WITH_TIMEOUT(model.currentPath(), directory.path(), 2000);
    QCOMPARE(window->property("activeTabIndex").toInt(), 0);

    QTest::keyClick(window, Qt::Key_Slash);
    QVERIFY(window->property("filterModeActive").toBool());
    for (const QChar character : QStringLiteral("alpha"))
        QTest::keyClick(window, static_cast<Qt::Key>(Qt::Key_A + character.unicode() - QLatin1Char('a').unicode()));
    QTRY_COMPARE(model.filterText(), QStringLiteral("alpha"));
    QTRY_COMPARE(model.rowCount(), 1);
    QTest::keyClick(window, Qt::Key_Escape);
    QVERIFY(!window->property("filterModeActive").toBool());
    QCOMPARE(model.filterText(), QString());
    QTRY_COMPARE(model.rowCount(), 3);
    QTRY_VERIFY(!window->property("filterPromptFocused").toBool());

    window->requestActivate();
    QTest::keyClick(window, Qt::Key_Period);
    QTRY_VERIFY(model.showHidden());
    QTRY_COMPARE(model.rowCount(), 4);

    QTest::keyClick(window, Qt::Key_Question, Qt::ShiftModifier);
    QTRY_VERIFY(window->property("keyReferenceVisible").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!window->property("keyReferenceVisible").toBool());

    QTest::keyClick(window, Qt::Key_Home);
    QTest::keyClick(window, Qt::Key_X);
    QCOMPARE(window->property("selectedItemCount").toInt(), 1);
    QTest::keyClick(window, Qt::Key_J);
    QTest::keyClick(window, Qt::Key_V);
    QVERIFY(window->property("visualModeActive").toBool());
    QCOMPARE(window->property("selectedItemCount").toInt(), 2);
    QTest::keyClick(window, Qt::Key_J);
    QCOMPARE(window->property("selectedItemCount").toInt(), 3);
    QTest::keyClick(window, Qt::Key_V);
    QVERIFY(!window->property("visualModeActive").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QCOMPARE(window->property("selectedItemCount").toInt(), 0);

    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    QCOMPARE(window->property("selectedItemCount").toInt(), model.rowCount());
    QTest::keyClick(window, Qt::Key_Escape);
    QCOMPARE(window->property("selectedItemCount").toInt(), 0);

    QTest::keyClick(window, Qt::Key_N);
    QTRY_VERIFY(window->property("operationPromptFocused").toBool());
    typeText(window, QStringLiteral("created-folder"));
    QCOMPARE(window->property("operationPromptText").toString(), QStringLiteral("created-folder"));
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(!window->property("operationPromptFocused").toBool());
    const QString createdPath = directory.filePath(QStringLiteral("created-folder"));
    QTRY_VERIFY_WITH_TIMEOUT(model.indexOfPath(createdPath) >= 0, 3000);
    QTRY_COMPARE(model.pathAt(window->property("currentItemIndex").toInt()), createdPath);

    QTest::keyClick(window, Qt::Key_U);
    QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(createdPath), 3000);
    QTest::keyClick(window, Qt::Key_N);
    QTRY_VERIFY(window->property("operationPromptFocused").toBool());
    typeText(window, QStringLiteral("created-folder"));
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(model.indexOfPath(createdPath) >= 0, 3000);
    QTRY_COMPARE(model.pathAt(window->property("currentItemIndex").toInt()), createdPath);

    QTest::keyClick(window, Qt::Key_F2);
    QTRY_VERIFY(window->property("operationPromptFocused").toBool());
    typeText(window, QStringLiteral("renamed-folder"));
    QTest::keyClick(window, Qt::Key_Return);
    const QString renamedPath = directory.filePath(QStringLiteral("renamed-folder"));
    QTRY_VERIFY_WITH_TIMEOUT(model.indexOfPath(renamedPath) >= 0, 3000);
    QVERIFY(!QFileInfo::exists(createdPath));

    QTest::keyClick(window, Qt::Key_F2);
    QTRY_VERIFY(window->property("operationPromptFocused").toBool());
    typeText(window, QStringLiteral("alpha.txt"));
    QTest::keyClick(window, Qt::Key_Return);
    QVERIFY(window->property("operationPromptFocused").toBool());
    QCOMPARE(window->property("operationPromptText").toString(), QStringLiteral("alpha.txt"));
    QVERIFY(model.errorMessage().contains(QStringLiteral("already exists")));
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!window->property("operationPromptFocused").toBool());

    QTest::keyClick(window, Qt::Key_Y);
    QCOMPARE(window->property("pendingSequence").toString(), QStringLiteral("y"));
    QTest::keyClick(window, Qt::Key_Y);
    QCOMPARE(window->property("clipboardItemCount").toInt(), 1);
    QCOMPARE(window->property("clipboardOperation").toString(), QStringLiteral("copy"));
    QCOMPARE(model.fileClipboardPaths(), QStringList{renamedPath});
    QVERIFY(!model.fileClipboardMove());
    QTest::keyClick(window, Qt::Key_P);
    const QString copiedPath = directory.filePath(QStringLiteral("renamed-folder (copy)"));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(copiedPath).isDir(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QCOMPARE(window->property("clipboardItemCount").toInt(), 1);

    QTRY_COMPARE(model.pathAt(window->property("currentItemIndex").toInt()), copiedPath);
    QTest::keyClick(window, Qt::Key_D);
    QCOMPARE(window->property("pendingSequence").toString(), QStringLiteral("d"));
    QTest::keyClick(window, Qt::Key_D);
    QCOMPARE(window->property("clipboardOperation").toString(), QStringLiteral("move"));
    QCOMPARE(window->property("clipboardItemCount").toInt(), 1);
    QTest::keyClick(window, Qt::Key_Escape);
    QCOMPARE(window->property("clipboardOperation").toString(), QString());
    QCOMPARE(window->property("clipboardItemCount").toInt(), 0);

    QTest::keyClick(window, Qt::Key_D);
    QTest::keyClick(window, Qt::Key_D);
    QCOMPARE(window->property("clipboardOperation").toString(), QStringLiteral("move"));
    QVERIFY(model.navigateTo(directory.filePath(QStringLiteral("folder"))));
    QTRY_COMPARE(model.currentPath(), directory.filePath(QStringLiteral("folder")));
    QTest::keyClick(window, Qt::Key_P);
    const QString movedPath = QDir(model.currentPath()).filePath(QStringLiteral("renamed-folder (copy)"));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(movedPath).isDir(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QVERIFY(!QFileInfo::exists(copiedPath));
    QCOMPARE(window->property("clipboardItemCount").toInt(), 0);

    const QString alphaPath = directory.filePath(QStringLiteral("alpha.txt"));
    const QString conflictingAlpha = QDir(directory.filePath(QStringLiteral("folder")))
                                         .filePath(QStringLiteral("alpha.txt"));
    writeFile(conflictingAlpha, "existing alpha");
    QVERIFY(model.navigateTo(directory.path()));
    QTRY_VERIFY_WITH_TIMEOUT(model.indexOfPath(alphaPath) >= 0, 3000);
    QTest::keyClick(window, Qt::Key_Home);
    for (int step = 0; step < model.rowCount() && model.pathAt(
             window->property("currentItemIndex").toInt()) != alphaPath; ++step) {
        QTest::keyClick(window, Qt::Key_J);
    }
    QCOMPARE(model.pathAt(window->property("currentItemIndex").toInt()), alphaPath);
    QTest::keyClick(window, Qt::Key_Y);
    QTest::keyClick(window, Qt::Key_Y);
    QCOMPARE(window->property("clipboardOperation").toString(), QStringLiteral("copy"));

    QVERIFY(model.navigateTo(directory.filePath(QStringLiteral("folder"))));
    QTRY_COMPARE(model.currentPath(), directory.filePath(QStringLiteral("folder")));
    QTest::keyClick(window, Qt::Key_P);
    QTRY_VERIFY_WITH_TIMEOUT(window->property("conflictPromptVisible").toBool(), 3000);
    QTest::keyClick(window, Qt::Key_N);
    QTRY_VERIFY(window->property("conflictRenamePromptFocused").toBool());
    typeText(window, QStringLiteral("alpha-renamed.txt"));
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QCOMPARE(readFile(QDir(model.currentPath()).filePath(QStringLiteral("alpha-renamed.txt"))),
             QByteArray("alpha"));

    QTest::keyClick(window, Qt::Key_P);
    QTRY_VERIFY_WITH_TIMEOUT(window->property("conflictPromptVisible").toBool(), 3000);
    QTest::keyClick(window, Qt::Key_A);
    QTest::keyClick(window, Qt::Key_S);
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QCOMPARE(readFile(conflictingAlpha), QByteArray("existing alpha"));

    QTest::keyClick(window, Qt::Key_P);
    QTRY_VERIFY_WITH_TIMEOUT(window->property("conflictPromptVisible").toBool(), 3000);
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QCOMPARE(readFile(conflictingAlpha), QByteArray("existing alpha"));

    QTest::keyClick(window, Qt::Key_Home);
    const QString pathToTrash = model.pathAt(window->property("currentItemIndex").toInt());
    QVERIFY(QFileInfo::exists(pathToTrash));
    QTest::keyClick(window, Qt::Key_D, Qt::ShiftModifier);
    QVERIFY(window->property("trashConfirmationVisible").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QVERIFY(!window->property("trashConfirmationVisible").toBool());
    QVERIFY(QFileInfo::exists(pathToTrash));

    QTest::keyClick(window, Qt::Key_D, Qt::ShiftModifier);
    QVERIFY(window->property("trashConfirmationVisible").toBool());
    QTest::keyClick(window, Qt::Key_Y);
    QTRY_VERIFY_WITH_TIMEOUT(!model.trashActive(), 5000);
    QVERIFY(!QFileInfo::exists(pathToTrash));
    QTRY_VERIFY_WITH_TIMEOUT(model.rowCount() > 0, 3000);
    QTRY_VERIFY(window->property("currentItemIndex").toInt() >= 0);

    const QString trashedName = QFileInfo(pathToTrash).fileName();
    QTest::keyClick(window, Qt::Key_T, Qt::ShiftModifier);
    QTRY_VERIFY_WITH_TIMEOUT(model.trashView(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.data(model.index(row), FileSystemModel::NameRole).toString()
                    == trashedName
                && model.data(model.index(row), FileSystemModel::TypeTextRole).toString()
                    == directory.filePath(QStringLiteral("folder"))) {
                return true;
            }
        }
        return false;
    }(), 3000);
    QTest::keyClick(window, Qt::Key_AsciiTilde);
    QTRY_VERIFY_WITH_TIMEOUT(!model.trashView(), 3000);
    QCOMPARE(model.currentPath(), QDir::homePath());
    QVERIFY(model.navigateTo(directory.filePath(QStringLiteral("folder"))));
    QTest::keyClick(window, Qt::Key_T, Qt::ShiftModifier);
    QTRY_VERIFY_WITH_TIMEOUT(model.trashView(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.data(model.index(row), FileSystemModel::NameRole).toString()
                == trashedName)
                return true;
        }
        return false;
    }(), 3000);
    QTest::keyClick(window, Qt::Key_Home);
    for (int step = 0; step < model.rowCount(); ++step) {
        const int row = window->property("currentItemIndex").toInt();
        if (row >= 0
            && model.data(model.index(row), FileSystemModel::NameRole).toString() == trashedName)
            break;
        QTest::keyClick(window, Qt::Key_J);
    }
    QCOMPARE(model.data(model.index(window->property("currentItemIndex").toInt()),
                        FileSystemModel::NameRole).toString(),
             trashedName);
    QTest::keyClick(window, Qt::Key_R);
    QTRY_VERIFY_WITH_TIMEOUT(!model.transferActive(), 5000);
    QVERIFY(QFileInfo::exists(pathToTrash));
    QTest::keyClick(window, Qt::Key_H);
    QTRY_VERIFY_WITH_TIMEOUT(!model.trashView(), 3000);
    QCOMPARE(model.currentPath(), directory.filePath(QStringLiteral("folder")));

    const QString emptyMePath = QDir(model.currentPath()).filePath(QStringLiteral("empty-me.txt"));
    writeFile(emptyMePath, "empty me");
    QVERIFY(model.startTrash({emptyMePath}));
    QTRY_VERIFY_WITH_TIMEOUT(!model.trashActive(), 5000);
    QTest::keyClick(window, Qt::Key_T, Qt::ShiftModifier);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 3000);
    QTest::keyClick(window, Qt::Key_E, Qt::ShiftModifier);
    QTRY_VERIFY(window->property("emptyTrashConfirmationVisible").toBool());
    QTRY_VERIFY(window->property("emptyTrashPromptFocused").toBool());
    typeText(window, QStringLiteral("no"));
    QTest::keyClick(window, Qt::Key_Return);
    QVERIFY(window->property("emptyTrashConfirmationVisible").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QVERIFY(!window->property("emptyTrashConfirmationVisible").toBool());
    QTest::keyClick(window, Qt::Key_E, Qt::ShiftModifier);
    QTRY_VERIFY(window->property("emptyTrashPromptFocused").toBool());
    typeText(window, QStringLiteral("empty"));
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(!model.trashActive(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 0, 3000);
}

void CoreTest::largeDirectoryLoadsAndFiltersPromptly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    for (int index = 0; index < 10000; ++index) {
        QFile file(directory.filePath(QStringLiteral("item-%1.dat").arg(index, 5, 10, QLatin1Char('0'))));
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    FileSystemModel model;
    QElapsedTimer timer;
    timer.start();
    QVERIFY(model.navigateTo(directory.path()));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 10000, 5000);
    const qint64 loadMilliseconds = timer.elapsed();
    QVERIFY2(loadMilliseconds < 5000,
             qPrintable(QStringLiteral("10,000-item load took %1 ms").arg(loadMilliseconds)));

    timer.restart();
    for (int index = 0; index < 10000; ++index) {
        const QString path = directory.filePath(
            QStringLiteral("item-%1.dat").arg(index, 5, 10, QLatin1Char('0')));
        QVERIFY(model.indexOfPath(path) >= 0);
    }
    const qint64 lookupMilliseconds = timer.elapsed();
    QVERIFY2(lookupMilliseconds < 1000,
             qPrintable(QStringLiteral("10,000 indexed path lookups took %1 ms")
                            .arg(lookupMilliseconds)));

    timer.restart();
    model.setFilterText(QStringLiteral("09999"));
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 1000);
    const qint64 filterMilliseconds = timer.elapsed();
    QVERIFY2(filterMilliseconds < 1000,
             qPrintable(QStringLiteral("10,000-item filter took %1 ms").arg(filterMilliseconds)));
}

QTEST_MAIN(CoreTest)

#include "test_core.moc"
