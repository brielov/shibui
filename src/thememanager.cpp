#include "thememanager.h"

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
QString unquoted(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2
        && ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
            || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))))
        return value.mid(1, value.size() - 2);
    return value;
}

QString withoutInlineComment(const QString &line)
{
    bool singleQuoted = false;
    bool doubleQuoted = false;
    for (int index = 0; index < line.size(); ++index) {
        const QChar character = line.at(index);
        if (character == QLatin1Char('\'') && !doubleQuoted)
            singleQuoted = !singleQuoted;
        else if (character == QLatin1Char('"') && !singleQuoted)
            doubleQuoted = !doubleQuoted;
        else if (character == QLatin1Char('#') && !singleQuoted && !doubleQuoted)
            return line.left(index);
    }
    return line;
}
}

ThemeManager::ThemeManager(const QString &themeDirectory, QObject *parent)
    : QObject(parent)
    , m_themeDirectory(themeDirectory.isEmpty()
          ? QDir::home().filePath(QStringLiteral(".local/state/omarchy/current/theme"))
          : themeDirectory)
{
    m_themeNamePath = themeDirectory.isEmpty()
        ? QFileInfo(m_themeDirectory).dir().filePath(QStringLiteral("theme.name"))
        : QDir(m_themeDirectory).filePath(QStringLiteral("theme.name"));

    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (!fixedFont.family().isEmpty())
        m_fontFamily = fixedFont.family();

    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(60);
    connect(&m_reloadTimer, &QTimer::timeout, this, &ThemeManager::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &ThemeManager::scheduleReload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &ThemeManager::scheduleReload);

    reload();
    if (!qEnvironmentVariableIsSet("SHIBUI_SKIP_DESKTOP_QUERIES"))
        QTimer::singleShot(0, this, &ThemeManager::queryDesktopValues);
}

QColor ThemeManager::background() const { return m_background; }
QColor ThemeManager::darkBackground() const { return m_darkBackground; }
QColor ThemeManager::lighterBackground() const { return m_lighterBackground; }
QColor ThemeManager::foreground() const { return m_foreground; }
QColor ThemeManager::muted() const { return m_muted; }
QColor ThemeManager::accent() const { return m_accent; }
QColor ThemeManager::selection() const { return m_selection; }
QColor ThemeManager::errorColor() const { return m_errorColor; }
QString ThemeManager::fontFamily() const { return m_fontFamily; }
qreal ThemeManager::fontSize() const { return m_fontSize; }
qreal ThemeManager::effectiveSpacingScale() const
{
    return m_spacingScale * (m_spacingScaleWithFont ? m_fontSize / 12.0 : 1.0);
}
int ThemeManager::cornerRadius() const { return m_cornerRadius; }
int ThemeManager::gapsOut() const { return m_gapsOut; }
QString ThemeManager::iconThemeName() const { return m_iconThemeName; }
QString ThemeManager::themeName() const { return m_themeName; }
qreal ThemeManager::hoverFillAlpha() const { return m_hoverFillAlpha; }
qreal ThemeManager::selectedFillAlpha() const { return m_selectedFillAlpha; }
qreal ThemeManager::hoverBorderAlpha() const { return m_hoverBorderAlpha; }
int ThemeManager::revision() const { return m_revision; }

void ThemeManager::reload()
{
    const TomlValues colors = readToml(QDir(m_themeDirectory).filePath(QStringLiteral("colors.toml")));
    const TomlValues shell = readToml(QDir(m_themeDirectory).filePath(QStringLiteral("shell.toml")));

    m_background = colorValue(colors, QStringLiteral("background"), m_background);
    m_darkBackground = colorValue(colors, QStringLiteral("dark_background"), m_darkBackground);
    m_lighterBackground = colorValue(colors, QStringLiteral("lighter_background"), m_lighterBackground);
    m_foreground = colorValue(colors, QStringLiteral("foreground"), m_foreground);
    const QColor muted = colorValue(colors, QStringLiteral("muted"), m_muted);
    m_muted = colors.value(QStringLiteral("mode")).trimmed().compare(
                  QStringLiteral("light"), Qt::CaseInsensitive) == 0
        ? colorValue(colors, QStringLiteral("light_foreground"), muted)
        : muted;
    m_accent = colorValue(colors, QStringLiteral("accent"), m_accent);
    m_selection = colorValue(colors, QStringLiteral("selection"), m_selection);
    m_errorColor = colorValue(colors, QStringLiteral("red"), m_errorColor);
    m_fontSize = qMax<qreal>(1.0, numberValue(shell, QStringLiteral("font.base-size"), m_fontSize));
    m_spacingScale = qMax<qreal>(0.1, numberValue(shell, QStringLiteral("spacing.scale"), m_spacingScale));
    m_spacingScaleWithFont = boolValue(shell, QStringLiteral("spacing.scale-with-font"),
                                       m_spacingScaleWithFont);
    m_hoverFillAlpha = qBound<qreal>(0.0,
        numberValue(shell, QStringLiteral("controls.hover-cursor-fill-alpha"), m_hoverFillAlpha), 1.0);
    m_selectedFillAlpha = qBound<qreal>(0.0,
        numberValue(shell, QStringLiteral("controls.selected-fill-alpha"), m_selectedFillAlpha), 1.0);
    m_hoverBorderAlpha = qBound<qreal>(0.0,
        numberValue(shell, QStringLiteral("controls.hover-cursor-border-alpha"), m_hoverBorderAlpha), 1.0);

    const QString iconName = readText(QDir(m_themeDirectory).filePath(QStringLiteral("icons.theme")));
    if (!iconName.isEmpty()) {
        m_iconThemeName = iconName;
        QIcon::setThemeName(iconName);
    }
    m_themeName = readText(m_themeNamePath);

    ++m_revision;
    emit themeChanged();
    emit revisionChanged();
    rebuildWatches();
}

ThemeManager::TomlValues ThemeManager::readToml(const QString &path)
{
    TomlValues values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return values;

    QString section;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            section = line.mid(1, line.size() - 2).trimmed();
            continue;
        }
        line = withoutInlineComment(line).trimmed();
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0)
            continue;
        const QString key = line.left(equals).trimmed();
        const QString qualified = section.isEmpty() ? key : section + QLatin1Char('.') + key;
        values.insert(qualified, unquoted(line.mid(equals + 1)));
    }
    return values;
}

QString ThemeManager::readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QColor ThemeManager::colorValue(const TomlValues &values, const QString &key, const QColor &fallback)
{
    const QColor candidate(values.value(key));
    return candidate.isValid() ? candidate : fallback;
}

qreal ThemeManager::numberValue(const TomlValues &values, const QString &key, qreal fallback)
{
    bool valid = false;
    const qreal value = values.value(key).toDouble(&valid);
    return valid ? value : fallback;
}

bool ThemeManager::boolValue(const TomlValues &values, const QString &key, bool fallback)
{
    const QString value = values.value(key).trimmed().toLower();
    if (value == QStringLiteral("true"))
        return true;
    if (value == QStringLiteral("false"))
        return false;
    return fallback;
}

void ThemeManager::rebuildWatches()
{
    const QStringList existing = m_watcher.files() + m_watcher.directories();
    if (!existing.isEmpty())
        m_watcher.removePaths(existing);

    QStringList paths;
    const QDir directory(m_themeDirectory);
    paths << directory.absolutePath();
    const QString parent = QFileInfo(directory.absolutePath()).dir().absolutePath();
    if (parent != directory.absolutePath())
        paths << parent;

    const QStringList fileNames = {
        QStringLiteral("colors.toml"),
        QStringLiteral("shell.toml"),
        QStringLiteral("icons.theme"),
    };
    for (const QString &fileName : fileNames) {
        const QString path = directory.filePath(fileName);
        if (QFileInfo::exists(path))
            paths << path;
    }
    if (QFileInfo::exists(m_themeNamePath))
        paths << m_themeNamePath;
    paths.removeDuplicates();
    if (!paths.isEmpty())
        m_watcher.addPaths(paths);
}

void ThemeManager::scheduleReload()
{
    m_reloadTimer.start();
}

void ThemeManager::queryDesktopValues()
{
    queryFont();
    queryHyprlandOption(QStringLiteral("decoration:rounding"));
    queryHyprlandOption(QStringLiteral("general:gaps_out"));
}

void ThemeManager::queryFont()
{
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus status) {
                const QString family = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                process->deleteLater();
                if (status != QProcess::NormalExit || exitCode != 0 || family.isEmpty()
                    || family == m_fontFamily)
                    return;
                m_fontFamily = family;
                ++m_revision;
                emit themeChanged();
                emit revisionChanged();
            });
    connect(process, &QProcess::errorOccurred, process,
            [process](QProcess::ProcessError processError) {
                if (processError == QProcess::FailedToStart)
                    process->deleteLater();
            });
    process->start(QStringLiteral("fc-match"),
                   {QStringLiteral("-f"), QStringLiteral("%{family[0]}"), QStringLiteral("monospace")});
}

void ThemeManager::queryHyprlandOption(const QString &option)
{
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this,
            [this, process, option](int exitCode, QProcess::ExitStatus status) {
                const QByteArray output = process->readAllStandardOutput();
                process->deleteLater();
                if (status != QProcess::NormalExit || exitCode != 0)
                    return;
                const QJsonObject object = QJsonDocument::fromJson(output).object();
                bool changed = false;
                if (option == QStringLiteral("decoration:rounding")) {
                    const int value = object.value(QStringLiteral("int")).toInt(m_cornerRadius);
                    if (value != m_cornerRadius) {
                        m_cornerRadius = qMax(0, value);
                        changed = true;
                    }
                } else if (option == QStringLiteral("general:gaps_out")) {
                    const QString css = object.value(QStringLiteral("css")).toString();
                    const QRegularExpressionMatch match = QRegularExpression(QStringLiteral("(-?\\d+)"))
                                                              .match(css);
                    if (match.hasMatch()) {
                        const int value = qMax(0, match.captured(1).toInt() / 2);
                        if (value != m_gapsOut) {
                            m_gapsOut = value;
                            changed = true;
                        }
                    }
                }
                if (changed) {
                    ++m_revision;
                    emit themeChanged();
                    emit revisionChanged();
                }
            });
    connect(process, &QProcess::errorOccurred, process,
            [process](QProcess::ProcessError processError) {
                if (processError == QProcess::FailedToStart)
                    process->deleteLater();
            });
    process->start(QStringLiteral("hyprctl"), {QStringLiteral("-j"), QStringLiteral("getoption"), option});
}
