#pragma once

#include "filetransfer.h"
#include "filetrash.h"
#include "filerestore.h"

#include <QAbstractListModel>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QHash>
#include <QMimeDatabase>
#include <QPersistentModelIndex>
#include <QTimer>

class FileSystemModel final : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int sortField READ sortField NOTIFY sortChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)
    Q_PROPERTY(QString homePath READ homePath CONSTANT)
    Q_PROPERTY(int iconRevision READ iconRevision WRITE setIconRevision NOTIFY iconRevisionChanged)
    Q_PROPERTY(bool transferActive READ transferActive NOTIFY transferChanged)
    Q_PROPERTY(qreal transferProgress READ transferProgress NOTIFY transferChanged)
    Q_PROPERTY(QString transferPhase READ transferPhase NOTIFY transferChanged)
    Q_PROPERTY(QString transferCurrentPath READ transferCurrentPath NOTIFY transferChanged)
    Q_PROPERTY(QString transferDestination READ transferDestination NOTIFY transferChanged)
    Q_PROPERTY(bool transferMove READ transferMove NOTIFY transferChanged)
    Q_PROPERTY(bool transferConflictActive READ transferConflictActive NOTIFY transferChanged)
    Q_PROPERTY(QString transferConflictSource READ transferConflictSource NOTIFY transferChanged)
    Q_PROPERTY(QString transferConflictTarget READ transferConflictTarget NOTIFY transferChanged)
    Q_PROPERTY(bool transferConflictSourceIsDirectory READ transferConflictSourceIsDirectory NOTIFY transferChanged)
    Q_PROPERTY(bool transferConflictTargetIsDirectory READ transferConflictTargetIsDirectory NOTIFY transferChanged)
    Q_PROPERTY(QString transferConflictError READ transferConflictError NOTIFY transferChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoChanged)
    Q_PROPERTY(QString undoDescription READ undoDescription NOTIFY undoChanged)
    Q_PROPERTY(bool undoActive READ undoActive NOTIFY undoChanged)
    Q_PROPERTY(QStringList fileClipboardPaths READ fileClipboardPaths NOTIFY fileClipboardChanged)
    Q_PROPERTY(bool fileClipboardMove READ fileClipboardMove NOTIFY fileClipboardChanged)
    Q_PROPERTY(bool trashView READ trashView NOTIFY trashViewChanged)
    Q_PROPERTY(bool trashActive READ trashActive NOTIFY trashChanged)
    Q_PROPERTY(qreal trashProgress READ trashProgress NOTIFY trashChanged)
    Q_PROPERTY(QString trashCurrentPath READ trashCurrentPath NOTIFY trashChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        DirectoryRole,
        SymlinkRole,
        BrokenSymlinkRole,
        SizeRole,
        SizeTextRole,
        TypeTextRole,
        ModifiedRole,
        ModifiedTextRole,
        IconSourceRole,
        ThumbnailSourceRole,
    };
    Q_ENUM(Role)

    enum SortField {
        NameSort = 0,
        SizeSort = 1,
        TypeSort = 2,
        ModifiedSort = 3,
    };
    Q_ENUM(SortField)

    explicit FileSystemModel(QObject *parent = nullptr);
    ~FileSystemModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentPath() const;
    QString filterText() const;
    void setFilterText(const QString &text);
    bool showHidden() const;
    void setShowHidden(bool show);
    bool loading() const;
    QString errorMessage() const;
    int sortField() const;
    bool sortAscending() const;
    QString homePath() const;
    int iconRevision() const;
    void setIconRevision(int revision);
    bool transferActive() const;
    qreal transferProgress() const;
    QString transferPhase() const;
    QString transferCurrentPath() const;
    QString transferDestination() const;
    bool transferMove() const;
    bool transferConflictActive() const;
    QString transferConflictSource() const;
    QString transferConflictTarget() const;
    bool transferConflictSourceIsDirectory() const;
    bool transferConflictTargetIsDirectory() const;
    QString transferConflictError() const;
    bool canUndo() const;
    QString undoDescription() const;
    bool undoActive() const;
    QStringList fileClipboardPaths() const;
    bool fileClipboardMove() const;
    bool trashView() const;
    bool trashActive() const;
    qreal trashProgress() const;
    QString trashCurrentPath() const;

    Q_INVOKABLE bool navigateTo(const QString &path);
    Q_INVOKABLE bool navigateToTrash();
    Q_INVOKABLE bool goParent();
    Q_INVOKABLE bool activate(int row);
    Q_INVOKABLE bool activatePath(const QString &path);
    Q_INVOKABLE bool openWith(const QString &path, const QString &desktopId);
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE bool isDirectoryAt(int row) const;
    Q_INVOKABLE int indexOfPath(const QString &path) const;
    Q_INVOKABLE QString createDirectory(const QString &name);
    Q_INVOKABLE bool startFolderWithSelection(const QString &name,
                                              const QStringList &paths);
    Q_INVOKABLE QString renamePath(const QString &path, const QString &newName);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE bool startTransfer(const QStringList &sources,
                                   const QString &destinationDirectory,
                                   bool move);
    Q_INVOKABLE void cancelTransfer();
    Q_INVOKABLE bool resolveTransferConflict(const QString &action,
                                             const QString &newName = QString(),
                                             bool applyToRemaining = false);
    Q_INVOKABLE bool startTrash(const QStringList &paths);
    Q_INVOKABLE void cancelTrash();
    Q_INVOKABLE bool startRestore(const QStringList &paths);
    Q_INVOKABLE bool startEmptyTrash();
    Q_INVOKABLE bool undoLast();
    Q_INVOKABLE void setFileClipboard(const QStringList &paths, bool move);
    Q_INVOKABLE void clearFileClipboardIfOwned();
    Q_INVOKABLE bool ownsFileClipboard() const;
    Q_INVOKABLE bool copyPathsAsText(const QStringList &paths);
    Q_INVOKABLE bool openTerminal(const QString &path);
    Q_INVOKABLE void setSort(int field, bool toggleWhenSame = true);
    Q_INVOKABLE void refresh();

signals:
    void currentPathChanged();
    void filterTextChanged();
    void showHiddenChanged();
    void loadingChanged();
    void errorMessageChanged();
    void countChanged();
    void sortChanged();
    void iconRevisionChanged();
    void transferChanged();
    void undoChanged();
    void undoFinished(bool success, const QString &message, const QStringList &paths);
    void fileClipboardChanged();
    void trashViewChanged();
    void contentsChanged();
    void fileLaunchFailed(const QString &message);
    void transferFinished(bool success,
                          bool cancelled,
                          const QString &message,
                          const QStringList &completedSources,
                          const QStringList &destinationPaths);
    void trashChanged();
    void trashFinished(bool success,
                       bool cancelled,
                       const QString &message,
                       const QStringList &trashedSources,
                       const QStringList &trashPaths,
                       const QStringList &failedPaths);

private:
    static QString normalizedPath(const QString &path, const QString &basePath = QString());
    bool isRootParent(const QModelIndex &parent) const;
    QFileInfo fileInfoAt(int row) const;
    QString typeText(const QFileInfo &info) const;
    QString iconSource(const QString &path) const;
    void launchFile(const QFileInfo &info);
    void scheduleRebuild();
    void rebuildRows();
    void updateSourceFilter();
    void updatePathWatches();
    void handlePathWatchChange();
    void setLoading(bool loading);
    void setErrorMessage(const QString &message);
    void updateTransferProgress(const TransferUpdate &update);
    void clearTransferConflict();
    void loadTrashMetadata();
    TransferConflictResolver makeTransferConflictResolver(
        const std::shared_ptr<std::atomic_bool> &cancelled);

    enum class UndoKind { CreateDirectory, Rename, Move, MoveIntoNewFolder, Trash };
    struct UndoAction {
        UndoKind kind;
        QStringList beforePaths;
        QStringList afterPaths;
        QStringList replacedTargetPaths;
        QStringList replacedTrashPaths;
        QStringList replacedTrashInfoPaths;
        QString createdContainerPath;
    };
    void pushUndo(UndoAction action);
    void finishUndo(const TransferResult &result);
    void removeTrashUndoActions();

    struct TransferConflictControl;

    QFileSystemModel m_source;
    QFileSystemWatcher m_pathWatcher;
    QModelIndex m_rootIndex;
    QVector<QPersistentModelIndex> m_rows;
    QVector<QFileInfo> m_trashRows;
    QHash<QString, int> m_rowByPath;
    QTimer m_rebuildTimer;
    mutable QMimeDatabase m_mimeDatabase;
    QString m_currentPath;
    QString m_filterText;
    QString m_errorMessage;
    bool m_showHidden = false;
    bool m_loading = false;
    int m_sortField = NameSort;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    int m_iconRevision = 0;
    QFutureWatcher<TransferResult> m_transferWatcher;
    std::shared_ptr<std::atomic_bool> m_transferCancelled;
    std::shared_ptr<TransferConflictControl> m_transferConflictControl;
    qreal m_transferProgress = 0;
    QString m_transferPhase;
    QString m_transferCurrentPath;
    QString m_transferDestination;
    bool m_transferActive = false;
    bool m_transferMove = false;
    bool m_transferRestore = false;
    QString m_transferCreatedDirectory;
    QVector<UndoAction> m_undoHistory;
    bool m_undoActive = false;
    bool m_transferConflictActive = false;
    QString m_transferConflictSource;
    QString m_transferConflictTarget;
    bool m_transferConflictSourceIsDirectory = false;
    bool m_transferConflictTargetIsDirectory = false;
    QString m_transferConflictError;
    QFutureWatcher<QHash<QString, QStringList>> m_trashMetadataWatcher;
    QHash<QString, QStringList> m_trashMetadata;
    QString m_pathBeforeTrash;
    QStringList m_trashFilesPaths;
    QStringList m_trashInfoPaths;
    bool m_trashView = false;
    bool m_trashMetadataReloadPending = false;
    QFutureWatcher<TrashResult> m_trashWatcher;
    std::shared_ptr<std::atomic_bool> m_trashCancelled;
    qreal m_trashProgress = 0;
    QString m_trashCurrentPath;
    bool m_trashActive = false;
    bool m_emptyTrashOperation = false;
};
