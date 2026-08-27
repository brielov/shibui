#pragma once

#include <QAbstractListModel>
#include <QProcess>
#include <QVector>

struct ApplicationEntry
{
    QString name;
    QString desktopId;
    bool isDefault = false;
};

class OpenWithModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(QString mimeType READ mimeType NOTIFY pathChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        DesktopIdRole,
        DefaultRole,
    };
    Q_ENUM(Role)

    explicit OpenWithModel(QObject *parent = nullptr);
    ~OpenWithModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool active() const;
    bool loading() const;
    QString path() const;
    QString mimeType() const;
    QString errorMessage() const;

    Q_INVOKABLE bool open(const QString &path);
    Q_INVOKABLE void close();
    Q_INVOKABLE QString desktopIdAt(int row) const;
    Q_INVOKABLE bool setDefault(int row);

signals:
    void activeChanged();
    void loadingChanged();
    void pathChanged();
    void errorMessageChanged();
    void countChanged();
    void defaultChanged(bool success, const QString &message);

private:
    void stopProcess();
    void setLoading(bool loading);
    void setErrorMessage(const QString &message);
    void replaceApplications(QVector<ApplicationEntry> applications);

    bool m_active = false;
    bool m_loading = false;
    QString m_path;
    QString m_mimeType;
    QString m_errorMessage;
    QVector<ApplicationEntry> m_applications;
    QProcess *m_process = nullptr;
};
