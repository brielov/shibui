#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

class ArchiveModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString description READ description NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)

public:
    explicit ArchiveModel(QObject *parent = nullptr);
    ~ArchiveModel() override;

    bool active() const;
    QString description() const;
    QString errorMessage() const;

    Q_INVOKABLE bool supportsArchive(const QString &path) const;
    Q_INVOKABLE bool createArchive(const QStringList &paths,
                                   const QString &destinationDirectory,
                                   const QString &name);
    Q_INVOKABLE bool extractArchive(const QString &path,
                                    const QString &destinationDirectory);
    Q_INVOKABLE void cancel();

signals:
    void changed();
    void finished(bool success, const QString &message, const QString &outputPath);

private:
    bool start(const QStringList &arguments, const QString &workingDirectory,
               const QString &outputPath, const QString &workPath, bool extracting);
    void finishProcess(QProcess *process, bool success, bool cancelled,
                       const QString &detail);
    void fail(const QString &message);
    static QString archiveBaseName(const QString &fileName);

    QProcess *m_process = nullptr;
    QString m_description;
    QString m_errorMessage;
    QString m_outputPath;
    QString m_workPath;
    bool m_extracting = false;
};
