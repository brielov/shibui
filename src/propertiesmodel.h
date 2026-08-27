#pragma once

#include <QFutureWatcher>
#include <QObject>

#include <atomic>
#include <memory>

struct FileProperties
{
    int generation = 0;
    QString name;
    QString path;
    QString mimeType;
    QString type;
    QString size;
    QString modified;
    QString created;
    QString accessed;
    QString owner;
    QString group;
    QString permissions;
    QString symlinkTarget;
    QString filesystemFree;
    bool directory = false;
    bool permissionsEditable = false;
    int permissionMode = 0;
    QString error;
};

struct DirectorySizeResult
{
    int generation = 0;
    quint64 bytes = 0;
    quint64 items = 0;
    bool cancelled = false;
    QString error;
};

class PropertiesModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool sizing READ sizing NOTIFY changed)
    Q_PROPERTY(bool directory READ directory NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString path READ path NOTIFY changed)
    Q_PROPERTY(QString mimeType READ mimeType NOTIFY changed)
    Q_PROPERTY(QString type READ type NOTIFY changed)
    Q_PROPERTY(QString size READ size NOTIFY changed)
    Q_PROPERTY(QString modified READ modified NOTIFY changed)
    Q_PROPERTY(QString created READ created NOTIFY changed)
    Q_PROPERTY(QString accessed READ accessed NOTIFY changed)
    Q_PROPERTY(QString owner READ owner NOTIFY changed)
    Q_PROPERTY(QString group READ group NOTIFY changed)
    Q_PROPERTY(QString permissions READ permissions NOTIFY changed)
    Q_PROPERTY(QString symlinkTarget READ symlinkTarget NOTIFY changed)
    Q_PROPERTY(QString filesystemFree READ filesystemFree NOTIFY changed)
    Q_PROPERTY(QString recursiveSize READ recursiveSize NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
    Q_PROPERTY(bool permissionsEditable READ permissionsEditable NOTIFY changed)
    Q_PROPERTY(int permissionMode READ permissionMode NOTIFY changed)

public:
    explicit PropertiesModel(QObject *parent = nullptr);
    ~PropertiesModel() override;

    bool active() const;
    bool loading() const;
    bool sizing() const;
    bool directory() const;
    QString name() const;
    QString path() const;
    QString mimeType() const;
    QString type() const;
    QString size() const;
    QString modified() const;
    QString created() const;
    QString accessed() const;
    QString owner() const;
    QString group() const;
    QString permissions() const;
    QString symlinkTarget() const;
    QString filesystemFree() const;
    QString recursiveSize() const;
    QString errorMessage() const;
    bool permissionsEditable() const;
    int permissionMode() const;

    Q_INVOKABLE bool open(const QString &path);
    Q_INVOKABLE void close();
    Q_INVOKABLE bool calculateDirectorySize();
    Q_INVOKABLE void cancelDirectorySize();
    Q_INVOKABLE bool togglePermissionBit(int index);

signals:
    void activeChanged();
    void changed();

private:
    void cancelWork();

    bool m_active = false;
    bool m_loading = false;
    bool m_sizing = false;
    int m_generation = 0;
    FileProperties m_properties;
    QString m_recursiveSize;
    std::shared_ptr<std::atomic_bool> m_sizeCancelled;
    QFutureWatcher<FileProperties> m_propertiesWatcher;
    QFutureWatcher<DirectorySizeResult> m_sizeWatcher;
};
