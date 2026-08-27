#include "previewmodel.h"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrentRun>

namespace {
QString fileIconSource(const QString &path)
{
    const QByteArray encoded = path.toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QStringLiteral("image://fileicon/%1").arg(QString::fromLatin1(encoded));
}
}

PreviewModel::PreviewModel(QObject *parent)
    : QObject(parent)
{
    connect(&m_readWatcher, &QFutureWatcher<PreviewReadResult>::finished, this, [this] {
        const PreviewReadResult result = m_readWatcher.result();
        if (result.generation != m_generation || !m_active)
            return;
        m_kind = result.kind;
        m_text = result.text;
        m_errorMessage = result.error;
        setLoading(false);
        emit contentChanged();
    });
}

PreviewModel::~PreviewModel()
{
    cancelWork();
    m_readWatcher.waitForFinished();
}

bool PreviewModel::active() const { return m_active; }
bool PreviewModel::loading() const { return m_loading; }
QString PreviewModel::path() const { return m_path; }
QString PreviewModel::name() const { return m_name; }
QString PreviewModel::kind() const { return m_kind; }
QString PreviewModel::text() const { return m_text; }
QString PreviewModel::imageSource() const { return m_imageSource; }
QString PreviewModel::iconSource() const { return m_iconSource; }
QString PreviewModel::errorMessage() const { return m_errorMessage; }

bool PreviewModel::open(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return false;

    cancelWork();
    const int generation = ++m_generation;
    m_path = info.absoluteFilePath();
    m_name = info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName();
    m_kind = QStringLiteral("unsupported");
    m_text.clear();
    m_imageSource.clear();
    m_iconSource = fileIconSource(m_path);
    m_errorMessage.clear();
    m_active = true;
    emit activeChanged();

    if (info.isDir()) {
        setLoading(false);
        emit contentChanged();
        return true;
    }

    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchExtension);
    if (mime.name().startsWith(QStringLiteral("image/"))) {
        m_kind = QStringLiteral("image");
        m_imageSource = QUrl::fromLocalFile(m_path).toString();
        setLoading(false);
        emit contentChanged();
    } else if (mime.name() == QStringLiteral("application/pdf")) {
        m_kind = QStringLiteral("pdf");
        startPdfPreview(m_path, generation);
    } else if (mime.name().startsWith(QStringLiteral("text/"))
               || mime.inherits(QStringLiteral("application/json"))
               || mime.inherits(QStringLiteral("application/xml"))) {
        m_kind = QStringLiteral("text");
        startTextPreview(m_path, generation);
    } else {
        setLoading(false);
        emit contentChanged();
    }
    return true;
}

void PreviewModel::close()
{
    if (!m_active)
        return;
    cancelWork();
    m_active = false;
    m_loading = false;
    emit activeChanged();
    emit contentChanged();
}

void PreviewModel::cancelWork()
{
    ++m_generation;
    if (m_pdfProcess) {
        QProcess *process = m_pdfProcess;
        m_pdfProcess = nullptr;
        process->disconnect(this);
        process->kill();
        process->deleteLater();
    }
    if (!m_pdfOutputPath.isEmpty()) {
        QFile::remove(m_pdfOutputPath);
        m_pdfOutputPath.clear();
    }
}

void PreviewModel::startTextPreview(const QString &path, int generation)
{
    setLoading(true);
    m_readWatcher.setFuture(QtConcurrent::run([path, generation] {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return PreviewReadResult{generation, QStringLiteral("unsupported"), {},
                                     file.errorString()};
        }
        QByteArray contents = file.read(512 * 1024 + 1);
        if (contents.contains('\0'))
            return PreviewReadResult{generation, QStringLiteral("unsupported"), {}, {}};
        const bool truncated = contents.size() > 512 * 1024;
        if (truncated)
            contents.truncate(512 * 1024);
        QString text = QString::fromUtf8(contents);
        if (truncated)
            text += QStringLiteral("\n\n… preview truncated …");
        return PreviewReadResult{generation, QStringLiteral("text"), text, {}};
    }));
}

void PreviewModel::startPdfPreview(const QString &path, int generation)
{
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("pdftoppm"));
    if (executable.isEmpty() || !m_temporaryDirectory.isValid()) {
        m_kind = QStringLiteral("unsupported");
        setLoading(false);
        emit contentChanged();
        return;
    }

    setLoading(true);
    const QString outputPrefix = m_temporaryDirectory.filePath(
        QStringLiteral("preview-%1").arg(generation));
    const QString outputPath = outputPrefix + QStringLiteral(".png");
    auto *process = new QProcess(this);
    m_pdfProcess = process;
    connect(process, &QProcess::finished, this,
            [this, process, generation, outputPrefix](int exitCode,
                                                       QProcess::ExitStatus exitStatus) {
                if (process != m_pdfProcess || generation != m_generation) {
                    process->deleteLater();
                    return;
                }
                m_pdfProcess = nullptr;
                if (exitStatus == QProcess::NormalExit && exitCode == 0
                    && QFileInfo::exists(outputPrefix + QStringLiteral(".png"))) {
                    m_pdfOutputPath = outputPrefix + QStringLiteral(".png");
                    m_imageSource = QUrl::fromLocalFile(m_pdfOutputPath).toString()
                        + QStringLiteral("?v=%1").arg(generation);
                } else {
                    QFile::remove(outputPrefix + QStringLiteral(".png"));
                    m_kind = QStringLiteral("unsupported");
                    m_errorMessage = QString::fromUtf8(
                        process->readAllStandardError()).trimmed();
                }
                process->deleteLater();
                setLoading(false);
                emit contentChanged();
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process, generation, outputPath](QProcess::ProcessError processError) {
                if (processError != QProcess::FailedToStart
                    || process != m_pdfProcess || generation != m_generation)
                    return;
                m_pdfProcess = nullptr;
                m_kind = QStringLiteral("unsupported");
                m_errorMessage = process->errorString();
                QFile::remove(outputPath);
                process->deleteLater();
                setLoading(false);
                emit contentChanged();
            });
    process->start(executable,
                   {QStringLiteral("-f"), QStringLiteral("1"),
                    QStringLiteral("-l"), QStringLiteral("1"),
                    QStringLiteral("-singlefile"), QStringLiteral("-scale-to"),
                    QStringLiteral("1400"), QStringLiteral("-png"), path, outputPrefix},
                   QIODevice::ReadOnly);
}

void PreviewModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit contentChanged();
}
