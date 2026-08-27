#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QVector>

struct TemplateEntry
{
    QString name;
    QString path;
    QString relativePath;
};

struct TemplateCopyResult
{
    bool success = false;
    QString message;
    QString outputPath;
};

class TemplateModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool copying READ copying NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        RelativePathRole,
    };
    Q_ENUM(Role)

    explicit TemplateModel(const QString &templatesPath = QString(), QObject *parent = nullptr);
    ~TemplateModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool active() const;
    bool loading() const;
    bool copying() const;
    QString errorMessage() const;
    Q_INVOKABLE bool begin(const QString &destinationDirectory);
    Q_INVOKABLE void close();
    Q_INVOKABLE QString suggestedNameAt(int row) const;
    Q_INVOKABLE bool createFrom(int row, const QString &name);

signals:
    void changed();
    void countChanged();
    void finished(bool success, const QString &message, const QString &outputPath);

private:
    static QVector<TemplateEntry> loadTemplates(const QString &rootPath);
    static TemplateCopyResult copyTemplate(const QString &sourcePath,
                                           const QString &outputPath);

    QString m_templatesPath;
    QString m_destinationDirectory;
    QVector<TemplateEntry> m_entries;
    QFutureWatcher<QVector<TemplateEntry>> m_loadWatcher;
    QFutureWatcher<TemplateCopyResult> m_copyWatcher;
    QString m_errorMessage;
    bool m_active = false;
    bool m_loading = false;
    bool m_copying = false;
};
