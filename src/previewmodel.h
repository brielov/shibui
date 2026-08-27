#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>

struct PreviewReadResult
{
    int generation = 0;
    QString kind;
    QString text;
    QString error;
};

class PreviewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY contentChanged)
    Q_PROPERTY(QString path READ path NOTIFY contentChanged)
    Q_PROPERTY(QString name READ name NOTIFY contentChanged)
    Q_PROPERTY(QString kind READ kind NOTIFY contentChanged)
    Q_PROPERTY(QString text READ text NOTIFY contentChanged)
    Q_PROPERTY(QString imageSource READ imageSource NOTIFY contentChanged)
    Q_PROPERTY(QString iconSource READ iconSource NOTIFY contentChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY contentChanged)

public:
    explicit PreviewModel(QObject *parent = nullptr);
    ~PreviewModel() override;

    bool active() const;
    bool loading() const;
    QString path() const;
    QString name() const;
    QString kind() const;
    QString text() const;
    QString imageSource() const;
    QString iconSource() const;
    QString errorMessage() const;

    Q_INVOKABLE bool open(const QString &path);
    Q_INVOKABLE void close();

signals:
    void activeChanged();
    void contentChanged();

private:
    void cancelWork();
    void startTextPreview(const QString &path, int generation);
    void startPdfPreview(const QString &path, int generation);
    void setLoading(bool loading);

    bool m_active = false;
    bool m_loading = false;
    int m_generation = 0;
    QString m_path;
    QString m_name;
    QString m_kind;
    QString m_text;
    QString m_imageSource;
    QString m_iconSource;
    QString m_errorMessage;
    QFutureWatcher<PreviewReadResult> m_readWatcher;
    QProcess *m_pdfProcess = nullptr;
    QString m_pdfOutputPath;
    QTemporaryDir m_temporaryDirectory;
};
