#include "omarchytheme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QMap>
#include <QSet>
#include <QTimer>

#include <cmath>

struct OmarchyTheme::Private {
    QFileSystemWatcher *watcher = nullptr;
    QTimer *debounce = nullptr;
    bool loaded = false;
};

namespace {

QString g_userThemes;
QString g_systemThemes;
QString g_currentState;

const QStringList kCanonicalColors = {
    QStringLiteral("background"),      QStringLiteral("foreground"),
    QStringLiteral("accent"),          QStringLiteral("muted"),
    QStringLiteral("selection"),       QStringLiteral("red"),
    QStringLiteral("orange"),          QStringLiteral("yellow"),
    QStringLiteral("green"),           QStringLiteral("cyan"),
    QStringLiteral("blue"),            QStringLiteral("magenta"),
    QStringLiteral("brown"),           QStringLiteral("bright_red"),
    QStringLiteral("bright_orange"),   QStringLiteral("bright_yellow"),
    QStringLiteral("bright_green"),    QStringLiteral("bright_cyan"),
    QStringLiteral("bright_blue"),     QStringLiteral("bright_magenta"),
    QStringLiteral("bright_brown"),    QStringLiteral("dark_background"),
    QStringLiteral("dark_foreground")
};

QString userThemesDir()
{
    if (!g_userThemes.isEmpty())
        return g_userThemes;
    return QDir::homePath() + QStringLiteral("/.config/omarchy/themes");
}

QString systemThemesDir()
{
    if (!g_systemThemes.isEmpty())
        return g_systemThemes;
    return QStringLiteral("/usr/share/omarchy/themes");
}

QString currentStateDir()
{
    if (!g_currentState.isEmpty())
        return g_currentState;
    return QDir::homePath() + QStringLiteral("/.local/state/omarchy/current");
}

QString readUtf8File(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

bool isHexChar(QChar c)
{
    return (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
        || (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
        || (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
}

QString hexOf(int r, int g, int b)
{
    return QStringLiteral("#%1%2%3")
        .arg(qBound(0, r, 255), 2, 16, QLatin1Char('0'))
        .arg(qBound(0, g, 255), 2, 16, QLatin1Char('0'))
        .arg(qBound(0, b, 255), 2, 16, QLatin1Char('0'))
        .toLower();
}

QString normalizeColor(QString value)
{
    value = value.trimmed();
    if (value.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
        value = value.mid(2);
    else if (value.startsWith(QLatin1Char('#')))
        value = value.mid(1);

    if (value.isEmpty())
        return {};
    for (const QChar c : value) {
        if (!isHexChar(c))
            return {};
    }

    if (value.size() == 3) {
        QString expanded;
        expanded.reserve(6);
        for (const QChar c : value) {
            expanded += c;
            expanded += c;
        }
        value = expanded;
    } else if (value.size() == 8) {
        value = value.left(6);
    } else if (value.size() != 6) {
        return {};
    }

    return QLatin1Char('#') + value.toLower();
}

int hexPair(const QString &hex, int index)
{
    return hex.mid(index, 2).toInt(nullptr, 16);
}

// Mix two #rrggbb colours toward `end` by `amount` (0..1). Same rounding as
// omarchy-theme-color so the app and the desktop agree on derived shades.
QString mixHex(const QString &start, const QString &end, qreal amount)
{
    const QString a = normalizeColor(start);
    const QString b = normalizeColor(end);
    if (a.isEmpty() || b.isEmpty())
        return {};
    amount = qBound(0.0, amount, 1.0);
    const int r = int(hexPair(a, 1) * (1.0 - amount) + hexPair(b, 1) * amount + 0.5);
    const int g = int(hexPair(a, 3) * (1.0 - amount) + hexPair(b, 3) * amount + 0.5);
    const int bl = int(hexPair(a, 5) * (1.0 - amount) + hexPair(b, 5) * amount + 0.5);
    return hexOf(r, g, bl);
}

QString extractTomlValue(QString value)
{
    value = value.trimmed();
    const int doubleQuote = value.indexOf(QLatin1Char('"'));
    const int singleQuote = value.indexOf(QLatin1Char('\''));
    int quote = -1;
    QChar delim;
    if (doubleQuote >= 0 && (singleQuote < 0 || doubleQuote < singleQuote)) {
        quote = doubleQuote;
        delim = QLatin1Char('"');
    } else if (singleQuote >= 0) {
        quote = singleQuote;
        delim = QLatin1Char('\'');
    }
    if (quote >= 0) {
        const int end = value.indexOf(delim, quote + 1);
        if (end > quote)
            return value.mid(quote + 1, end - quote - 1);
    }

    // Unquoted #rrggbb is a colour, not a TOML comment.
    if (value.startsWith(QLatin1Char('#'))) {
        int i = 1;
        while (i < value.size() && isHexChar(value.at(i)))
            ++i;
        return value.left(i);
    }

    const int comment = value.indexOf(QStringLiteral(" #"));
    if (comment >= 0)
        value = value.left(comment).trimmed();
    const int space = value.indexOf(QLatin1Char(' '));
    if (space >= 0)
        value = value.left(space);
    const int tab = value.indexOf(QLatin1Char('\t'));
    if (tab >= 0)
        value = value.left(tab);
    return value;
}

QMap<QString, QString> parseTomlMap(QString text)
{
    QMap<QString, QString> out;
    if (text.startsWith(QChar(0xFEFF)))
        text = text.mid(1);

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;
        if (trimmed.startsWith(QLatin1Char('[')))
            continue;

        const int equals = trimmed.indexOf(QLatin1Char('='));
        if (equals <= 0)
            continue;

        QString key = trimmed.left(equals).trimmed();
        key.remove(QLatin1Char('"'));
        key.remove(QLatin1Char('\''));
        key.remove(QLatin1Char(' '));
        if (key.contains(QLatin1Char('.')))
            key = key.section(QLatin1Char('.'), -1);
        if (key.isEmpty() || key.startsWith(QLatin1Char('#')))
            continue;

        out.insert(key, extractTomlValue(trimmed.mid(equals + 1)));
    }
    return out;
}

bool hasColor(const QMap<QString, QString> &colors, const QString &key)
{
    return !colors.value(key).isEmpty();
}

void aliasColor(QMap<QString, QString> &colors, const QString &key, const QString &fallback)
{
    if (!hasColor(colors, key) && hasColor(colors, fallback))
        colors.insert(key, colors.value(fallback));
}

void putIfEmpty(QMap<QString, QString> &colors, const QString &key, const QString &value)
{
    if (!hasColor(colors, key) && !value.isEmpty())
        colors.insert(key, value);
}

void seedCore(QMap<QString, QString> &colors)
{
    putIfEmpty(colors, QStringLiteral("background"), QStringLiteral("#101010"));
    putIfEmpty(colors, QStringLiteral("foreground"), QStringLiteral("#eeeeee"));
}

void seedHues(QMap<QString, QString> &colors)
{
    putIfEmpty(colors, QStringLiteral("red"), QStringLiteral("#c05050"));
    putIfEmpty(colors, QStringLiteral("yellow"), QStringLiteral("#c0a040"));
    putIfEmpty(colors, QStringLiteral("green"), QStringLiteral("#50a050"));
    putIfEmpty(colors, QStringLiteral("cyan"), QStringLiteral("#40a0a0"));
    putIfEmpty(colors, QStringLiteral("blue"), QStringLiteral("#5584aa"));
    putIfEmpty(colors, QStringLiteral("magenta"), QStringLiteral("#a050a0"));
}

void resolveColors(QMap<QString, QString> &colors)
{
    // Short-name palette used by older themes.
    aliasColor(colors, QStringLiteral("background"), QStringLiteral("bg"));
    aliasColor(colors, QStringLiteral("dark_background"), QStringLiteral("dark_bg"));
    aliasColor(colors, QStringLiteral("foreground"), QStringLiteral("fg"));
    aliasColor(colors, QStringLiteral("dark_foreground"), QStringLiteral("dark_fg"));
    aliasColor(colors, QStringLiteral("bright_foreground"), QStringLiteral("bright_fg"));

    // Terminal-form files name the 16 ANSI slots and omit semantic roles.
    putIfEmpty(colors, QStringLiteral("background"), colors.value(QStringLiteral("color0")));
    putIfEmpty(colors, QStringLiteral("foreground"), colors.value(QStringLiteral("color7")));
    seedCore(colors);

    // Keep ANSI consumers in sync with the semantic pair (omarchy-theme-color).
    if (hasColor(colors, QStringLiteral("background")))
        colors.insert(QStringLiteral("color0"), colors.value(QStringLiteral("background")));
    if (hasColor(colors, QStringLiteral("foreground")))
        colors.insert(QStringLiteral("color7"), colors.value(QStringLiteral("foreground")));

    aliasColor(colors, QStringLiteral("red"), QStringLiteral("color1"));
    aliasColor(colors, QStringLiteral("green"), QStringLiteral("color2"));
    aliasColor(colors, QStringLiteral("yellow"), QStringLiteral("color3"));
    aliasColor(colors, QStringLiteral("blue"), QStringLiteral("color4"));
    aliasColor(colors, QStringLiteral("magenta"), QStringLiteral("color5"));
    aliasColor(colors, QStringLiteral("cyan"), QStringLiteral("color6"));
    aliasColor(colors, QStringLiteral("bright_red"), QStringLiteral("color9"));
    aliasColor(colors, QStringLiteral("bright_green"), QStringLiteral("color10"));
    aliasColor(colors, QStringLiteral("bright_yellow"), QStringLiteral("color11"));
    aliasColor(colors, QStringLiteral("bright_blue"), QStringLiteral("color12"));
    aliasColor(colors, QStringLiteral("bright_magenta"), QStringLiteral("color13"));
    aliasColor(colors, QStringLiteral("bright_cyan"), QStringLiteral("color14"));
    aliasColor(colors, QStringLiteral("magenta"), QStringLiteral("purple"));
    aliasColor(colors, QStringLiteral("bright_magenta"), QStringLiteral("bright_purple"));
    seedHues(colors);

    aliasColor(colors, QStringLiteral("accent"), QStringLiteral("blue"));
    putIfEmpty(colors, QStringLiteral("accent"), QStringLiteral("#5584aa"));
    putIfEmpty(colors, QStringLiteral("bright_foreground"),
               colors.value(QStringLiteral("color15"), colors.value(QStringLiteral("foreground"))));
    putIfEmpty(colors, QStringLiteral("dark_foreground"),
               colors.value(QStringLiteral("color8"), colors.value(QStringLiteral("foreground"))));
    putIfEmpty(colors, QStringLiteral("muted"),
               colors.value(QStringLiteral("color8"), colors.value(QStringLiteral("dark_foreground"))));
    putIfEmpty(colors, QStringLiteral("muted"), QStringLiteral("#666666"));
    putIfEmpty(colors, QStringLiteral("selection"),
               colors.value(QStringLiteral("selection_background"),
                            colors.value(QStringLiteral("color8"),
                                         colors.value(QStringLiteral("background")))));
    putIfEmpty(colors, QStringLiteral("selection"), QStringLiteral("#186a9a"));
    aliasColor(colors, QStringLiteral("orange"), QStringLiteral("yellow"));
    putIfEmpty(colors, QStringLiteral("brown"),
               mixHex(colors.value(QStringLiteral("orange")), QStringLiteral("#000000"), 0.5));

    putIfEmpty(colors, QStringLiteral("dark_background"),
               mixHex(colors.value(QStringLiteral("background")), QStringLiteral("#000000"), 0.25));
    putIfEmpty(colors, QStringLiteral("bright_red"),
               mixHex(colors.value(QStringLiteral("red")), QStringLiteral("#ffffff"), 0.20));
    putIfEmpty(colors, QStringLiteral("bright_yellow"),
               mixHex(colors.value(QStringLiteral("yellow")), QStringLiteral("#ffffff"), 0.20));
    putIfEmpty(colors, QStringLiteral("bright_green"),
               mixHex(colors.value(QStringLiteral("green")), QStringLiteral("#ffffff"), 0.20));
    putIfEmpty(colors, QStringLiteral("bright_cyan"),
               mixHex(colors.value(QStringLiteral("cyan")), QStringLiteral("#ffffff"), 0.20));
    putIfEmpty(colors, QStringLiteral("bright_blue"),
               mixHex(colors.value(QStringLiteral("blue")), QStringLiteral("#ffffff"), 0.20));
    putIfEmpty(colors, QStringLiteral("bright_magenta"),
               mixHex(colors.value(QStringLiteral("magenta")), QStringLiteral("#ffffff"), 0.20));
    putIfEmpty(colors, QStringLiteral("bright_orange"),
               mixHex(colors.value(QStringLiteral("orange")), QStringLiteral("#ffffff"), 0.20));
    putIfEmpty(colors, QStringLiteral("bright_brown"),
               mixHex(colors.value(QStringLiteral("brown")), QStringLiteral("#ffffff"), 0.20));

    aliasColor(colors, QStringLiteral("color0"), QStringLiteral("background"));
    aliasColor(colors, QStringLiteral("color1"), QStringLiteral("red"));
    aliasColor(colors, QStringLiteral("color2"), QStringLiteral("green"));
    aliasColor(colors, QStringLiteral("color3"), QStringLiteral("yellow"));
    aliasColor(colors, QStringLiteral("color4"), QStringLiteral("blue"));
    aliasColor(colors, QStringLiteral("color5"), QStringLiteral("magenta"));
    aliasColor(colors, QStringLiteral("color6"), QStringLiteral("cyan"));
    aliasColor(colors, QStringLiteral("color7"), QStringLiteral("foreground"));
    aliasColor(colors, QStringLiteral("color8"), QStringLiteral("muted"));
    aliasColor(colors, QStringLiteral("color9"), QStringLiteral("bright_red"));
    aliasColor(colors, QStringLiteral("color10"), QStringLiteral("bright_green"));
    aliasColor(colors, QStringLiteral("color11"), QStringLiteral("bright_yellow"));
    aliasColor(colors, QStringLiteral("color12"), QStringLiteral("bright_blue"));
    aliasColor(colors, QStringLiteral("color13"), QStringLiteral("bright_magenta"));
    aliasColor(colors, QStringLiteral("color14"), QStringLiteral("bright_cyan"));
    aliasColor(colors, QStringLiteral("color15"), QStringLiteral("bright_foreground"));

    if (!hasColor(colors, QStringLiteral("mode"))) {
        const QString type = colors.value(QStringLiteral("theme_type")).toLower();
        if (type == QStringLiteral("dark") || type == QStringLiteral("light"))
            colors.insert(QStringLiteral("mode"), type);
    }
    if (!hasColor(colors, QStringLiteral("mode"))) {
        const QString bg = colors.value(QStringLiteral("background"));
        if (bg.size() == 7) {
            const int sum = hexPair(bg, 1) + hexPair(bg, 3) + hexPair(bg, 5);
            colors.insert(QStringLiteral("mode"),
                          sum > 382 ? QStringLiteral("light") : QStringLiteral("dark"));
        } else {
            colors.insert(QStringLiteral("mode"), QStringLiteral("dark"));
        }
    }
}

qreal linearizeSrgb(qreal channel)
{
    if (channel <= 0.04045)
        return channel / 12.92;
    return std::pow((channel + 0.055) / 1.055, 2.4);
}

qreal relativeLuminance(const QString &hex)
{
    const QString n = normalizeColor(hex);
    if (n.isEmpty())
        return 0.0;
    return 0.2126 * linearizeSrgb(hexPair(n, 1) / 255.0)
        + 0.7152 * linearizeSrgb(hexPair(n, 3) / 255.0)
        + 0.0722 * linearizeSrgb(hexPair(n, 5) / 255.0);
}

QString colorToHex(const QColor &color)
{
    return hexOf(color.red(), color.green(), color.blue());
}

bool isSafeThemeName(const QString &name)
{
    if (name.isEmpty() || name == QLatin1Char('.') || name == QStringLiteral(".."))
        return false;
    return !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\'));
}

QString themeSlug(QString name)
{
    name = name.trimmed().toLower();
    name.replace(QLatin1Char(' '), QLatin1Char('-'));
    return name;
}

void collectThemeNames(const QString &dirPath, QSet<QString> *seen, QStringList *names)
{
    const QDir dir(dirPath);
    if (!dir.exists())
        return;
    const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : entries) {
        const QString name = info.fileName();
        if (!isSafeThemeName(name) || seen->contains(name))
            continue;
        seen->insert(name);
        names->append(name);
    }
}

} // namespace

OmarchyTheme::OmarchyTheme(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->watcher = new QFileSystemWatcher(this);
    d->debounce = new QTimer(this);
    d->debounce->setSingleShot(true);
    d->debounce->setInterval(150);

    connect(d->watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
        d->debounce->start();
    });
    connect(d->watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        d->debounce->start();
    });
    connect(d->debounce, &QTimer::timeout, this, [this]() {
        // Re-arm watches first: replacing the theme directory drops inotify
        // watches on the old colors.toml.
        watchThemeLink();
        reload();
    });

    reload();
    watchThemeLink();
}

OmarchyTheme::~OmarchyTheme()
{
    delete d;
}

QString OmarchyTheme::name() const
{
    return m_name;
}

void OmarchyTheme::setOverrideTheme(const QString &themeName)
{
    if (m_overrideTheme == themeName)
        return;
    m_overrideTheme = themeName;
    reload();
}

QString OmarchyTheme::overrideTheme() const
{
    return m_overrideTheme;
}

QJsonObject OmarchyTheme::palette() const
{
    return m_palette;
}

QString OmarchyTheme::backgroundImagePath() const
{
    return m_backgroundImagePath;
}

QStringList OmarchyTheme::installedThemes()
{
    QSet<QString> seen;
    QStringList names;
    collectThemeNames(userThemesDir(), &seen, &names);
    collectThemeNames(systemThemesDir(), &seen, &names);
    names.sort(Qt::CaseInsensitive);
    return names;
}

void OmarchyTheme::setDirectoriesForTest(const QString &userThemes, const QString &systemThemes,
                                         const QString &currentState)
{
    g_userThemes = userThemes;
    g_systemThemes = systemThemes;
    g_currentState = currentState;
}

QJsonObject OmarchyTheme::parseColorsToml(const QString &tomlText)
{
    const QMap<QString, QString> raw = parseTomlMap(tomlText);
    QMap<QString, QString> colors;

    for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
        const QString key = it.key();
        const QString value = it.value().trimmed();
        if (key == QStringLiteral("mode") || key == QStringLiteral("theme_type")) {
            const QString mode = value.toLower();
            if (mode == QStringLiteral("dark") || mode == QStringLiteral("light"))
                colors.insert(key, mode);
            continue;
        }
        const QString hex = normalizeColor(value);
        if (!hex.isEmpty())
            colors.insert(key, hex);
    }

    resolveColors(colors);

    QJsonObject palette;
    const QString mode = colors.value(QStringLiteral("mode"));
    palette.insert(QStringLiteral("mode"),
                   (mode == QStringLiteral("light")) ? QStringLiteral("light")
                                                     : QStringLiteral("dark"));
    for (const QString &key : kCanonicalColors) {
        QString value = colors.value(key);
        if (value.isEmpty())
            value = QStringLiteral("#101010");
        palette.insert(key, value);
    }

    QJsonArray ansi;
    for (int i = 0; i < 16; ++i) {
        QString value = colors.value(QStringLiteral("color%1").arg(i));
        if (value.isEmpty())
            value = (i < 8) ? colors.value(QStringLiteral("background"))
                            : colors.value(QStringLiteral("foreground"));
        if (value.isEmpty())
            value = QStringLiteral("#101010");
        ansi.append(value);
    }
    palette.insert(QStringLiteral("ansi"), ansi);
    return palette;
}

double OmarchyTheme::contrastRatio(const QString &foreground, const QString &background)
{
    if (normalizeColor(foreground).isEmpty() || normalizeColor(background).isEmpty())
        return 1.0;
    const qreal l1 = relativeLuminance(foreground);
    const qreal l2 = relativeLuminance(background);
    const qreal lighter = qMax(l1, l2);
    const qreal darker = qMin(l1, l2);
    return (lighter + 0.05) / (darker + 0.05);
}

QString OmarchyTheme::ensureContrast(const QString &foreground, const QString &background,
                                     double minRatio)
{
    if (contrastRatio(foreground, background) >= minRatio)
        return foreground;

    const QString fgHex = normalizeColor(foreground);
    const QString bgHex = normalizeColor(background);
    if (fgHex.isEmpty() || bgHex.isEmpty())
        return foreground;

    QColor fg(fgHex);
    // Qt 6's getHslF takes float*, not qreal*.
    float h = 0, s = 0, l = 0, a = 1;
    fg.getHslF(&h, &s, &l, &a);
    if (h < 0)
        h = 0;

    // Walk lightness toward the end of the axis farther from the background
    // so contrast is monotonic once we pick a direction.
    const qreal dir = (relativeLuminance(bgHex) < 0.5) ? 1.0 : -1.0;
    QString best = fgHex;
    for (int i = 1; i <= 255; ++i) {
        const float newL = qBound(0.0f, l + static_cast<float>(dir) * (i / 255.0f), 1.0f);
        QColor candidate;
        candidate.setHslF(h, s, newL, a);
        const QString hex = colorToHex(candidate);
        best = hex;
        if (contrastRatio(hex, bgHex) >= minRatio)
            return hex;
        if (newL == 0.0f || newL == 1.0f)
            break;
    }
    return best;
}

void OmarchyTheme::reload()
{
    const QString current = currentStateDir();
    QString colorsPath;
    QString name;

    if (!m_overrideTheme.isEmpty()) {
        const QString slug = themeSlug(m_overrideTheme);
        const QString userPath = userThemesDir() + QLatin1Char('/') + slug
            + QStringLiteral("/colors.toml");
        const QString systemPath = systemThemesDir() + QLatin1Char('/') + slug
            + QStringLiteral("/colors.toml");
        if (isSafeThemeName(slug) && QFileInfo::exists(userPath)) {
            colorsPath = userPath;
            name = slug;
        } else if (isSafeThemeName(slug) && QFileInfo::exists(systemPath)) {
            colorsPath = systemPath;
            name = slug;
        } else {
            qWarning("OmarchyTheme: unknown theme '%s', falling back to the live theme",
                     qUtf8Printable(m_overrideTheme));
        }
    }

    if (colorsPath.isEmpty()) {
        colorsPath = current + QStringLiteral("/theme/colors.toml");
        name = readUtf8File(current + QStringLiteral("/theme.name")).trimmed();
        if (name.isEmpty()) {
            const QString canonical = QFileInfo(current + QStringLiteral("/theme")).canonicalFilePath();
            name = QFileInfo(canonical).fileName();
        }
        if (name.isEmpty())
            name = QStringLiteral("unknown");
    }

    const QJsonObject palette = parseColorsToml(readUtf8File(colorsPath));

    QString bgPath = current + QStringLiteral("/background");
    if (!QFileInfo::exists(bgPath))
        bgPath.clear();

    const bool changed = name != m_name || palette != m_palette || bgPath != m_backgroundImagePath;
    m_name = name;
    m_palette = palette;
    m_backgroundImagePath = bgPath;

    if (d->loaded && changed)
        emit themeChanged();
    d->loaded = true;
}

void OmarchyTheme::watchThemeLink()
{
    const QStringList watched = d->watcher->files() + d->watcher->directories();
    if (!watched.isEmpty())
        d->watcher->removePaths(watched);

    const QString current = currentStateDir();
    const QString themeDir = current + QStringLiteral("/theme");
    const QStringList paths = {
        current,
        themeDir,
        themeDir + QStringLiteral("/colors.toml"),
        current + QStringLiteral("/theme.name"),
        current + QStringLiteral("/background")
    };
    for (const QString &path : paths) {
        if (QFileInfo::exists(path))
            d->watcher->addPath(path);
    }
}
