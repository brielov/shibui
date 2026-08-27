#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QProcess>
#include <QTimer>
#include <QVector>

struct PlaceEntry
{
    QString label;
    QString path;
    QString kind;
    QString devicePath;
    QString ejectPath;
    bool mounted = false;
    bool ejectable = false;
    QString networkUri;
};

class PlacesModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool deviceActionActive READ deviceActionActive NOTIFY deviceActionChanged)

public:
    enum Role {
        LabelRole = Qt::UserRole + 1,
        PathRole,
        KindRole,
        TrashRole,
        VolumeRole,
        BookmarkRole,
        DevicePathRole,
        MountedRole,
        EjectableRole,
        NetworkRole,
        NetworkUriRole,
        RecentRole,
        SectionRole,
    };
    Q_ENUM(Role)

    explicit PlacesModel(QObject *parent = nullptr);
    ~PlacesModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE bool isTrashAt(int row) const;
    Q_INVOKABLE bool isRecentAt(int row) const;
    Q_INVOKABLE bool isBookmarkAt(int row) const;
    Q_INVOKABLE QString labelAt(int row) const;
    bool deviceActionActive() const;
    Q_INVOKABLE bool isDeviceAt(int row) const;
    Q_INVOKABLE bool isMountedAt(int row) const;
    Q_INVOKABLE bool isEjectableAt(int row) const;
    Q_INVOKABLE bool mountAt(int row);
    Q_INVOKABLE bool unmountAt(int row);
    Q_INVOKABLE bool ejectAt(int row);
    Q_INVOKABLE bool isNetworkAt(int row) const;
    Q_INVOKABLE QString networkUriAt(int row) const;
    Q_INVOKABLE QString networkUriForPath(const QString &path) const;
    Q_INVOKABLE bool addNetworkBookmark(const QString &uri, const QString &label = QString());
    Q_INVOKABLE bool addBookmark(const QString &path, const QString &label = QString());
    Q_INVOKABLE bool renameBookmark(int row, const QString &label);
    Q_INVOKABLE bool removeBookmark(int row);
    Q_INVOKABLE bool moveBookmark(int row, int offset);

signals:
    void countChanged();
    void refreshed();
    void deviceActionChanged();
    void deviceActionFinished(bool success, const QString &message, const QString &path);

private:
    static QVector<PlaceEntry> collectPlaces(const QVector<PlaceEntry> &bookmarks,
                                             const QStringList &recentNetworkUris,
                                             const QVector<PlaceEntry> &knownDiscoveries,
                                             bool discoverNetworks);
    int bookmarkIndexForRow(int row) const;
    void saveBookmarks() const;
    bool startDeviceAction(int row, const QString &action);

    QVector<PlaceEntry> m_places;
    QVector<PlaceEntry> m_bookmarks;
    QFutureWatcher<QVector<PlaceEntry>> m_watcher;
    QTimer m_refreshTimer;
    QElapsedTimer m_discoveryAge;
    QVector<PlaceEntry> m_discoveredNetworks;
    bool m_refreshPending = false;
    QProcess *m_deviceProcess = nullptr;
};
