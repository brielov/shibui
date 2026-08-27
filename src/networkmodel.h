#pragma once

#include <QObject>
#include <QProcess>
#include <QSet>

class NetworkModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connecting READ connecting NOTIFY changed)
    Q_PROPERTY(bool promptActive READ promptActive NOTIFY changed)
    Q_PROPERTY(bool promptSecret READ promptSecret NOTIFY changed)
    Q_PROPERTY(QString promptText READ promptText NOTIFY changed)
    Q_PROPERTY(QString uri READ uri NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)

public:
    explicit NetworkModel(QObject *parent = nullptr);
    ~NetworkModel() override;

    bool connecting() const;
    bool promptActive() const;
    bool promptSecret() const;
    QString promptText() const;
    QString uri() const;
    QString errorMessage() const;

    Q_INVOKABLE bool connectTo(const QString &uri);
    Q_INVOKABLE void submitResponse(const QString &response);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool disconnectFrom(const QString &uri);

signals:
    void changed();
    void connected(const QString &path, const QString &uri, const QString &message);
    void finished(bool success, const QString &message);

private:
    static QString normalizedUri(const QString &uri);
    static QStringList mountedPaths();
    QString locateMountedPath() const;
    void startProcess(const QStringList &arguments, bool disconnecting);
    void finishProcess(QProcess *process, bool success);
    void consumeOutput();
    void saveRecentUri();

    QProcess *m_process = nullptr;
    bool m_connecting = false;
    bool m_promptActive = false;
    bool m_promptSecret = false;
    bool m_disconnecting = false;
    QString m_promptText;
    QString m_uri;
    QString m_errorMessage;
    QString m_transcript;
    QString m_promptBuffer;
    QSet<QString> m_mountsBefore;
};
