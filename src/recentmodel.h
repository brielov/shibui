#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QFutureWatcher>
#include <QVector>

struct RecentEntry
{
    QString name;
    QString path;
    QString parentPath;
    bool directory = false;
    QDateTime modified;
};

class RecentModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        RelativePathRole,
        DirectoryRole,
        IconSourceRole,
    };
    Q_ENUM(Role)

    explicit RecentModel(QObject *parent = nullptr);
    ~RecentModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const;
    QString errorMessage() const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE bool isDirectoryAt(int row) const;

signals:
    void countChanged();
    void changed();

private:
    struct LoadResult {
        QVector<RecentEntry> entries;
        QString error;
    };
    static LoadResult loadEntries();

    QVector<RecentEntry> m_entries;
    QFutureWatcher<LoadResult> m_watcher;
    QString m_errorMessage;
    bool m_loading = false;
};
