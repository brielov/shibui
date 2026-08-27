#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QVector>

struct BulkRenameEntry
{
    QString sourcePath;
    QString originalName;
    QString proposedName;
    QString error;
};

struct BulkRenameResult
{
    bool success = false;
    QString message;
    QStringList targetPaths;
};

class BulkRenameModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool applying READ applying NOTIFY changed)
    Q_PROPERTY(QString findText READ findText WRITE setFindText NOTIFY previewChanged)
    Q_PROPERTY(QString replacementText READ replacementText WRITE setReplacementText NOTIFY previewChanged)
    Q_PROPERTY(bool numbering READ numbering WRITE setNumbering NOTIFY previewChanged)
    Q_PROPERTY(bool canApply READ canApply NOTIFY previewChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
    Q_PROPERTY(int count READ rowCount NOTIFY previewChanged)

public:
    enum Role {
        OriginalNameRole = Qt::UserRole + 1,
        ProposedNameRole,
        ValidRole,
        ErrorRole,
    };
    Q_ENUM(Role)

    explicit BulkRenameModel(QObject *parent = nullptr);
    ~BulkRenameModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool active() const;
    bool applying() const;
    QString findText() const;
    void setFindText(const QString &text);
    QString replacementText() const;
    void setReplacementText(const QString &text);
    bool numbering() const;
    void setNumbering(bool numbering);
    bool canApply() const;
    QString errorMessage() const;

    Q_INVOKABLE bool begin(const QStringList &paths);
    Q_INVOKABLE void close();
    Q_INVOKABLE bool apply();

signals:
    void changed();
    void previewChanged();
    void finished(bool success, const QString &message, const QStringList &targetPaths);

private:
    void rebuildPreview();
    static BulkRenameResult renameAll(const QVector<BulkRenameEntry> &entries);

    QVector<BulkRenameEntry> m_entries;
    QFutureWatcher<BulkRenameResult> m_watcher;
    QString m_findText;
    QString m_replacementText;
    QString m_errorMessage;
    bool m_active = false;
    bool m_applying = false;
    bool m_numbering = false;
    bool m_canApply = false;
};
