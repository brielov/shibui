#include "fileiconprovider.h"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QMimeDatabase>
#include <QPainter>

FileIconProvider::FileIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap FileIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString encoded = id.section(QLatin1Char('?'), 0, 0);
    QColor tint;
    QString iconKey;
    QString iconName;
    QFileInfo fileInfo;
    bool themedRequest = false;
    if (encoded.startsWith(QStringLiteral("theme/"))) {
        const QStringList parts = encoded.split(QLatin1Char('/'));
        iconName = parts.value(1);
        iconKey = encoded;
        themedRequest = true;
        if (parts.size() > 2)
            tint = QColor(QLatin1Char('#') + parts.at(2));
    } else {
        const QByteArray decoded = QByteArray::fromBase64(
            encoded.toLatin1(), QByteArray::Base64UrlEncoding);
        fileInfo = QFileInfo(QString::fromUtf8(decoded));
        if (fileInfo.isDir()) {
            iconName = QStringLiteral("folder");
        } else {
            const QMimeType mime = QMimeDatabase().mimeTypeForFile(
                fileInfo, QMimeDatabase::MatchExtension);
            iconName = mime.iconName().isEmpty() ? mime.genericIconName() : mime.iconName();
        }
        iconKey = iconName.isEmpty()
            ? QStringLiteral("file:%1").arg(fileInfo.suffix().toCaseFolded()) : iconName;
    }

    QSize target = requestedSize.isValid() ? requestedSize : QSize(24, 24);
    target.setWidth(qBound(16, target.width(), 64));
    target.setHeight(qBound(16, target.height(), 64));
    const QString cacheKey = QStringLiteral("%1@%2x%3?%4")
                                 .arg(iconKey).arg(target.width()).arg(target.height())
                                 .arg(id.section(QLatin1Char('?'), 1));
    if (const QPixmap *cached = m_cache.object(cacheKey)) {
        if (size)
            *size = cached->size();
        return *cached;
    }

    QFileIconProvider provider;
    QIcon icon = QIcon::fromTheme(iconName);
    if (icon.isNull() && themedRequest) {
        if (iconName.contains(QStringLiteral("trash")))
            icon = provider.icon(QFileIconProvider::Trashcan);
        else if (iconName.contains(QStringLiteral("network")))
            icon = provider.icon(QFileIconProvider::Network);
        else if (iconName.contains(QStringLiteral("drive")))
            icon = provider.icon(QFileIconProvider::Drive);
        else
            icon = provider.icon(QFileIconProvider::Folder);
    } else if (icon.isNull()) {
        icon = provider.icon(fileInfo);
    }
    if (icon.isNull())
        icon = QIcon::fromTheme(fileInfo.isDir() ? QStringLiteral("folder")
                                                 : QStringLiteral("text-x-generic"));

    QPixmap pixmap = icon.pixmap(target);
    if (tint.isValid() && !pixmap.isNull()) {
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), tint);
    }
    if (size)
        *size = pixmap.size();
    m_cache.insert(cacheKey, new QPixmap(pixmap));
    return pixmap;
}
