#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

class ThemeManager final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QColor background READ background NOTIFY themeChanged)
    Q_PROPERTY(QColor darkBackground READ darkBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor lighterBackground READ lighterBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor foreground READ foreground NOTIFY themeChanged)
    Q_PROPERTY(QColor muted READ muted NOTIFY themeChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QColor selection READ selection NOTIFY themeChanged)
    Q_PROPERTY(QColor errorColor READ errorColor NOTIFY themeChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY themeChanged)
    Q_PROPERTY(qreal fontSize READ fontSize NOTIFY themeChanged)
    Q_PROPERTY(qreal effectiveSpacingScale READ effectiveSpacingScale NOTIFY themeChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius NOTIFY themeChanged)
    Q_PROPERTY(int gapsOut READ gapsOut NOTIFY themeChanged)
    Q_PROPERTY(QString iconThemeName READ iconThemeName NOTIFY themeChanged)
    Q_PROPERTY(QString themeName READ themeName NOTIFY themeChanged)
    Q_PROPERTY(qreal hoverFillAlpha READ hoverFillAlpha NOTIFY themeChanged)
    Q_PROPERTY(qreal selectedFillAlpha READ selectedFillAlpha NOTIFY themeChanged)
    Q_PROPERTY(qreal hoverBorderAlpha READ hoverBorderAlpha NOTIFY themeChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit ThemeManager(const QString &themeDirectory = QString(), QObject *parent = nullptr);

    QColor background() const;
    QColor darkBackground() const;
    QColor lighterBackground() const;
    QColor foreground() const;
    QColor muted() const;
    QColor accent() const;
    QColor selection() const;
    QColor errorColor() const;
    QString fontFamily() const;
    qreal fontSize() const;
    qreal effectiveSpacingScale() const;
    int cornerRadius() const;
    int gapsOut() const;
    QString iconThemeName() const;
    QString themeName() const;
    qreal hoverFillAlpha() const;
    qreal selectedFillAlpha() const;
    qreal hoverBorderAlpha() const;
    int revision() const;

    Q_INVOKABLE void reload();

signals:
    void themeChanged();
    void revisionChanged();

private:
    using TomlValues = QHash<QString, QString>;

    static TomlValues readToml(const QString &path);
    static QString readText(const QString &path);
    static QColor colorValue(const TomlValues &values, const QString &key, const QColor &fallback);
    static qreal numberValue(const TomlValues &values, const QString &key, qreal fallback);
    static bool boolValue(const TomlValues &values, const QString &key, bool fallback);
    void rebuildWatches();
    void scheduleReload();
    void queryDesktopValues();
    void queryFont();
    void queryHyprlandOption(const QString &option);

    QString m_themeDirectory;
    QString m_themeNamePath;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    QColor m_background{QStringLiteral("#111318")};
    QColor m_darkBackground{QStringLiteral("#0c0f12")};
    QColor m_lighterBackground{QStringLiteral("#1d242b")};
    QColor m_foreground{QStringLiteral("#d8dee9")};
    QColor m_muted{QStringLiteral("#7b8493")};
    QColor m_accent{QStringLiteral("#7aa2f7")};
    QColor m_selection{QStringLiteral("#29384f")};
    QColor m_errorColor{QStringLiteral("#f7768e")};
    QString m_fontFamily{QStringLiteral("monospace")};
    qreal m_fontSize = 12.0;
    qreal m_spacingScale = 1.0;
    bool m_spacingScaleWithFont = true;
    int m_cornerRadius = 0;
    int m_gapsOut = 5;
    QString m_iconThemeName;
    QString m_themeName;
    qreal m_hoverFillAlpha = 0.08;
    qreal m_selectedFillAlpha = 0.18;
    qreal m_hoverBorderAlpha = 0.25;
    int m_revision = 0;
};
