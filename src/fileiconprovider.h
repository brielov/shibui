#pragma once

#include <QCache>
#include <QPixmap>
#include <QQuickImageProvider>

class FileIconProvider final : public QQuickImageProvider
{
public:
    FileIconProvider();

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QCache<QString, QPixmap> m_cache{256};
};
