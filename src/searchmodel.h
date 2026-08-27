#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QProcess>
#include <QTimer>
#include <QVector>

struct SearchEntry
{
    QString name;
    QString path;
    QString relativePath;
    bool directory = false;
    int score = 0;
};

struct SearchCandidate
{
    QString relativePath;
    bool directory = false;
    qint64 modifiedMilliseconds = 0;
};

struct SearchRankResult
{
    int generation = 0;
    QVector<SearchEntry> entries;
};

class SearchModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString basePath READ basePath NOTIFY basePathChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(int scannedCount READ scannedCount NOTIFY scannedCountChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString typeFilter READ typeFilter WRITE setTypeFilter NOTIFY typeFilterChanged)
    Q_PROPERTY(int modifiedWithinDays READ modifiedWithinDays WRITE setModifiedWithinDays NOTIFY modifiedWithinDaysChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        RelativePathRole,
        DirectoryRole,
        IconSourceRole,
    };
    Q_ENUM(Role)

    explicit SearchModel(QObject *parent = nullptr);
    ~SearchModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const;
    void setQuery(const QString &query);
    QString basePath() const;
    bool scanning() const;
    QString errorMessage() const;
    int scannedCount() const;
    QString typeFilter() const;
    void setTypeFilter(const QString &typeFilter);
    int modifiedWithinDays() const;
    void setModifiedWithinDays(int days);

    Q_INVOKABLE bool start(const QString &basePath, bool showHidden);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE bool isDirectoryAt(int row) const;

signals:
    void queryChanged();
    void basePathChanged();
    void scanningChanged();
    void errorMessageChanged();
    void scannedCountChanged();
    void countChanged();
    void typeFilterChanged();
    void modifiedWithinDaysChanged();

private:
    void consumeOutput(QProcess *process, int generation, bool flush = false);
    void scheduleRanking();
    void startRanking();
    void replaceResults(QVector<SearchEntry> entries);
    void setScanning(bool scanning);
    void setErrorMessage(const QString &message);

    QString m_query;
    QString m_basePath;
    QString m_errorMessage;
    bool m_scanning = false;
    int m_scannedCount = 0;
    QString m_typeFilter = QStringLiteral("all");
    int m_modifiedWithinDays = 0;
    int m_scanGeneration = 0;
    int m_rankGeneration = 0;
    bool m_rankPending = false;
    QProcess *m_process = nullptr;
    QByteArray m_outputBuffer;
    QVector<QVector<SearchCandidate>> m_batches;
    QVector<SearchEntry> m_results;
    QTimer m_rankTimer;
    QFutureWatcher<SearchRankResult> m_rankWatcher;
};
