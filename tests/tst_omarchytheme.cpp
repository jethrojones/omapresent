#include <QtTest>

#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "testrunner.h"
#include "omarchytheme.h"

// Real colors.toml contents copied from themes on this machine. Tests never
// read the live system paths.

static const char kCatppuccin[] = R"toml(
mode = "dark"

accent = "#89b4fa"
selection = "#45475a"
muted = "#585b70"

background = "#1e1e2e"
dark_background = "#161622"
darker_background = "#101019"
lighter_background = "#313244"

foreground = "#cdd6f4"
dark_foreground = "#6c7086"
light_foreground = "#bac2de"
bright_foreground = "#cdd6f4"

red = "#f38ba8"
yellow = "#f9e2af"
orange = "#f6b6ab"
green = "#a6e3a1"
cyan = "#94e2d5"
blue = "#89b4fa"
magenta = "#f5c2e7"
brown = "#7b5b55"

bright_red = "#f38ba8"
bright_yellow = "#f9e2af"
bright_green = "#a6e3a1"
bright_cyan = "#94e2d5"
bright_blue = "#89b4fa"
bright_magenta = "#f5c2e7"
)toml";

static const char kGoldRush[] = R"toml(
accent = "#C9A227"
cursor = "#C9A227"
foreground = "#D9D9D9"
background = "#121212"
selection_foreground = "#D9D9D9"
selection_background = "#926C15"
color0 = "#76520E"
color1 = "#DBB42C"
color2 = "#A47E1B"
color3 = "#FAD643"
color4 = "#926C15"
color5 = "#B69121"
color6 = "#805B10"
color7 = "#EDC531"
color8 = "#805B10"
color9 = "#EDC531"
color10 = "#B69121"
color11 = "#FFEE69"
color12 = "#A47E1B"
color13 = "#C9A227"
color14 = "#DBB42C"
color15 = "#FAD643"
)toml";

static const char kLastHorizon[] = R"toml(
mode = "dark"

accent = "#b59790"
selection = "#584e51"
muted = "#584e51"

background = "#0c0b0c"
dark_background = "#090809"
darker_background = "#060606"
lighter_background = "#0c0b0c"

foreground = "#FAFCFB"
dark_foreground = "#584e51"
light_foreground = "#cfd3cd"
bright_foreground = "#e2dddc"

hyprland_active_border = "rgba(8a8588ee) rgba(e2dddcee)"
hyprland_inactive_border = "rgba(584e51aa)"
active_border_color = "#d6d3de"
active_tab_background = "#a5a0b6"

red = "#c38b7b"
yellow = "#6B5E73"
green = "#87a9b0"
cyan = "#a5a0b6"
blue = "#b59790"
magenta = "#c4d8e2"

bright_red = "#c38b7b"
bright_yellow = "#6B5E73"
bright_green = "#87a9b0"
bright_cyan = "#a5a0b6"
bright_blue = "#b59790"
bright_magenta = "#c4d8e2"
)toml";

static const char kFlexokiLight[] = R"toml(
mode = "light"

accent = "#205EA6"
selection = "#CECDC3"
muted = "#B7B5AC"

background = "#FFFCF0"
dark_background = "#f2efe4"
darker_background = "#e5e2d8"
lighter_background = "#E6E4D9"

foreground = "#100F0F"
dark_foreground = "#878580"
light_foreground = "#403E3C"
bright_foreground = "#100F0F"

red = "#D14D41"
yellow = "#D0A215"
orange = "#d0772b"
green = "#879A39"
cyan = "#3AA99F"
blue = "#205EA6"
magenta = "#CE5D97"
brown = "#683b15"

bright_red = "#D14D41"
bright_yellow = "#D0A215"
bright_green = "#879A39"
bright_cyan = "#3AA99F"
bright_blue = "#4385BE"
bright_magenta = "#CE5D97"
)toml";

static const char *const kCanonicalColorKeys[] = {
    "background",     "foreground",      "accent",          "muted",
    "selection",      "red",             "orange",          "yellow",
    "green",          "cyan",            "blue",            "magenta",
    "brown",          "bright_red",      "bright_orange",   "bright_yellow",
    "bright_green",   "bright_cyan",     "bright_blue",     "bright_magenta",
    "bright_brown",   "dark_background", "dark_foreground"
};

static bool isRrggbb(const QString &value)
{
    static const QRegularExpression re(QStringLiteral("^#[0-9a-f]{6}$"));
    return re.match(value).hasMatch();
}

static bool writeText(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    return file.write(contents) == contents.size();
}

struct TestDirs {
    QTemporaryDir tmp;
    QString user;
    QString system;
    QString current;

    bool init()
    {
        if (!tmp.isValid())
            return false;
        user = tmp.path() + QStringLiteral("/user");
        system = tmp.path() + QStringLiteral("/system");
        current = tmp.path() + QStringLiteral("/current");
        if (!QDir().mkpath(user) || !QDir().mkpath(system)
                || !QDir().mkpath(current + QStringLiteral("/theme")))
            return false;
        OmarchyTheme::setDirectoriesForTest(user, system, current);
        return true;
    }

    ~TestDirs() { OmarchyTheme::setDirectoriesForTest({}, {}, {}); }
};

class OmarchyThemeTest : public QObject {
    Q_OBJECT

private slots:
    void parseRichNamedForm();
    void parseTerminalFormDerivesAnsiRoles();
    void parseMissingOrangeBrown();
    void parseMixedCaseHex();
    void parseExpandsRgbShorthand();
    void parseAccepts0xAndBareHex();
    void parseTomlTables();
    void parseUnquotedAndComments();
    void parseMalformedYieldsDefaults();
    void parseAlwaysCanonicalKeys();
    void contrastBlackOnWhite();
    void contrastMidTonePairs();
    void ensureContrastAlreadySufficientUnchanged();
    void ensureContrastRaisesFailingPairKeepsHue();
    void installedThemesUserWinsSorted();
    void overrideResolutionOrder();
    void overrideUnknownFallsBackToLive();
    void liveReloadDebouncesAndEmitsOnce();
    void backgroundImagePathFromCurrent();

private:
    void assertCanonical(const QJsonObject &palette);
};

void OmarchyThemeTest::assertCanonical(const QJsonObject &palette)
{
    QVERIFY(palette.contains(QStringLiteral("mode")));
    const QString mode = palette.value(QStringLiteral("mode")).toString();
    QVERIFY(mode == QStringLiteral("dark") || mode == QStringLiteral("light"));

    for (const char *key : kCanonicalColorKeys) {
        QVERIFY2(palette.contains(QLatin1String(key)), key);
        QVERIFY2(isRrggbb(palette.value(QLatin1String(key)).toString()), key);
    }

    const QJsonArray ansi = palette.value(QStringLiteral("ansi")).toArray();
    QCOMPARE(ansi.size(), 16);
    for (int i = 0; i < 16; ++i) {
        QVERIFY2(isRrggbb(ansi.at(i).toString()),
                 qPrintable(QStringLiteral("ansi[%1]").arg(i)));
    }
}

void OmarchyThemeTest::parseRichNamedForm()
{
    const QJsonObject p = OmarchyTheme::parseColorsToml(QString::fromUtf8(kCatppuccin));
    assertCanonical(p);
    QCOMPARE(p.value(QStringLiteral("mode")).toString(), QStringLiteral("dark"));
    QCOMPARE(p.value(QStringLiteral("background")).toString(), QStringLiteral("#1e1e2e"));
    QCOMPARE(p.value(QStringLiteral("foreground")).toString(), QStringLiteral("#cdd6f4"));
    QCOMPARE(p.value(QStringLiteral("accent")).toString(), QStringLiteral("#89b4fa"));
    QCOMPARE(p.value(QStringLiteral("muted")).toString(), QStringLiteral("#585b70"));
    QCOMPARE(p.value(QStringLiteral("selection")).toString(), QStringLiteral("#45475a"));
    QCOMPARE(p.value(QStringLiteral("red")).toString(), QStringLiteral("#f38ba8"));
    QCOMPARE(p.value(QStringLiteral("orange")).toString(), QStringLiteral("#f6b6ab"));
    QCOMPARE(p.value(QStringLiteral("dark_background")).toString(), QStringLiteral("#161622"));
    QCOMPARE(p.value(QStringLiteral("dark_foreground")).toString(), QStringLiteral("#6c7086"));
    QCOMPARE(p.value(QStringLiteral("bright_orange")).toString(), QStringLiteral("#f8c5bc"));
    QCOMPARE(p.value(QStringLiteral("bright_brown")).toString(), QStringLiteral("#957c77"));

    const QJsonArray ansi = p.value(QStringLiteral("ansi")).toArray();
    QCOMPARE(ansi.at(0).toString(), QStringLiteral("#1e1e2e"));
    QCOMPARE(ansi.at(1).toString(), QStringLiteral("#f38ba8"));
    QCOMPARE(ansi.at(2).toString(), QStringLiteral("#a6e3a1"));
    QCOMPARE(ansi.at(7).toString(), QStringLiteral("#cdd6f4"));
    QCOMPARE(ansi.at(8).toString(), QStringLiteral("#585b70"));
    QCOMPARE(ansi.at(9).toString(), QStringLiteral("#f38ba8"));
    QCOMPARE(ansi.at(15).toString(), QStringLiteral("#cdd6f4"));
}

void OmarchyThemeTest::parseTerminalFormDerivesAnsiRoles()
{
    const QJsonObject p = OmarchyTheme::parseColorsToml(QString::fromUtf8(kGoldRush));
    assertCanonical(p);
    QCOMPARE(p.value(QStringLiteral("mode")).toString(), QStringLiteral("dark"));
    QCOMPARE(p.value(QStringLiteral("background")).toString(), QStringLiteral("#121212"));
    QCOMPARE(p.value(QStringLiteral("foreground")).toString(), QStringLiteral("#d9d9d9"));
    QCOMPARE(p.value(QStringLiteral("accent")).toString(), QStringLiteral("#c9a227"));
    QCOMPARE(p.value(QStringLiteral("muted")).toString(), QStringLiteral("#805b10"));
    QCOMPARE(p.value(QStringLiteral("selection")).toString(), QStringLiteral("#926c15"));
    QCOMPARE(p.value(QStringLiteral("red")).toString(), QStringLiteral("#dbb42c"));
    QCOMPARE(p.value(QStringLiteral("green")).toString(), QStringLiteral("#a47e1b"));
    QCOMPARE(p.value(QStringLiteral("yellow")).toString(), QStringLiteral("#fad643"));
    QCOMPARE(p.value(QStringLiteral("blue")).toString(), QStringLiteral("#926c15"));
    QCOMPARE(p.value(QStringLiteral("magenta")).toString(), QStringLiteral("#b69121"));
    QCOMPARE(p.value(QStringLiteral("cyan")).toString(), QStringLiteral("#805b10"));
    QCOMPARE(p.value(QStringLiteral("orange")).toString(), QStringLiteral("#fad643"));
    QCOMPARE(p.value(QStringLiteral("brown")).toString(), QStringLiteral("#7d6b22"));
    QCOMPARE(p.value(QStringLiteral("bright_red")).toString(), QStringLiteral("#edc531"));
    QCOMPARE(p.value(QStringLiteral("bright_yellow")).toString(), QStringLiteral("#ffee69"));
    QCOMPARE(p.value(QStringLiteral("dark_background")).toString(), QStringLiteral("#0e0e0e"));
    QCOMPARE(p.value(QStringLiteral("dark_foreground")).toString(), QStringLiteral("#805b10"));

    const QJsonArray ansi = p.value(QStringLiteral("ansi")).toArray();
    QCOMPARE(ansi.at(0).toString(), QStringLiteral("#121212"));
    QCOMPARE(ansi.at(1).toString(), QStringLiteral("#dbb42c"));
    QCOMPARE(ansi.at(7).toString(), QStringLiteral("#d9d9d9"));
    QCOMPARE(ansi.at(8).toString(), QStringLiteral("#805b10"));
    QCOMPARE(ansi.at(15).toString(), QStringLiteral("#fad643"));
}

void OmarchyThemeTest::parseMissingOrangeBrown()
{
    const QJsonObject p = OmarchyTheme::parseColorsToml(QString::fromUtf8(kLastHorizon));
    assertCanonical(p);
    QCOMPARE(p.value(QStringLiteral("orange")).toString(), QStringLiteral("#6b5e73"));
    QCOMPARE(p.value(QStringLiteral("brown")).toString(), QStringLiteral("#362f3a"));
    QCOMPARE(p.value(QStringLiteral("foreground")).toString(), QStringLiteral("#fafcfb"));
}

void OmarchyThemeTest::parseMixedCaseHex()
{
    const QJsonObject p = OmarchyTheme::parseColorsToml(QString::fromUtf8(kFlexokiLight));
    assertCanonical(p);
    QCOMPARE(p.value(QStringLiteral("mode")).toString(), QStringLiteral("light"));
    QCOMPARE(p.value(QStringLiteral("background")).toString(), QStringLiteral("#fffcf0"));
    QCOMPARE(p.value(QStringLiteral("accent")).toString(), QStringLiteral("#205ea6"));
    QCOMPARE(p.value(QStringLiteral("foreground")).toString(), QStringLiteral("#100f0f"));
}

void OmarchyThemeTest::parseExpandsRgbShorthand()
{
    const QJsonObject p = OmarchyTheme::parseColorsToml(
        QStringLiteral("background = \"#123\"\nforeground = \"#abc\"\naccent = \"#08f\"\n"));
    assertCanonical(p);
    QCOMPARE(p.value(QStringLiteral("background")).toString(), QStringLiteral("#112233"));
    QCOMPARE(p.value(QStringLiteral("foreground")).toString(), QStringLiteral("#aabbcc"));
    QCOMPARE(p.value(QStringLiteral("accent")).toString(), QStringLiteral("#0088ff"));
}

void OmarchyThemeTest::parseAccepts0xAndBareHex()
{
    const QJsonObject p = OmarchyTheme::parseColorsToml(QStringLiteral(
        "background = 0x102030\n"
        "foreground = \"0XFFEEDD\"\n"
        "accent = 89b4fa\n"
        "red = \"#f38ba8ff\"\n"));
    assertCanonical(p);
    QCOMPARE(p.value(QStringLiteral("background")).toString(), QStringLiteral("#102030"));
    QCOMPARE(p.value(QStringLiteral("foreground")).toString(), QStringLiteral("#ffeedd"));
    QCOMPARE(p.value(QStringLiteral("accent")).toString(), QStringLiteral("#89b4fa"));
    QCOMPARE(p.value(QStringLiteral("red")).toString(), QStringLiteral("#f38ba8"));
}

void OmarchyThemeTest::parseTomlTables()
{
    const QJsonObject p = OmarchyTheme::parseColorsToml(QStringLiteral(
        "[colors]\n"
        "background = \"#111111\"\n"
        "foreground = \"#eeeeee\"\n"
        "\n"
        "[colors.normal]\n"
        "red = \"#ff0000\"\n"
        "blue = \"#0000ff\"\n"));
    assertCanonical(p);
    QCOMPARE(p.value(QStringLiteral("background")).toString(), QStringLiteral("#111111"));
    QCOMPARE(p.value(QStringLiteral("foreground")).toString(), QStringLiteral("#eeeeee"));
    QCOMPARE(p.value(QStringLiteral("red")).toString(), QStringLiteral("#ff0000"));
    QCOMPARE(p.value(QStringLiteral("blue")).toString(), QStringLiteral("#0000ff"));
}

void OmarchyThemeTest::parseUnquotedAndComments()
{
    const QJsonObject p = OmarchyTheme::parseColorsToml(QStringLiteral(
        "# a file-level comment\n"
        "background = #102030 # unquoted hex\n"
        "foreground = \"#ccddee\" # quoted with trailing comment\n"
        "accent = '#89b4fa'\n"));
    assertCanonical(p);
    QCOMPARE(p.value(QStringLiteral("background")).toString(), QStringLiteral("#102030"));
    QCOMPARE(p.value(QStringLiteral("foreground")).toString(), QStringLiteral("#ccddee"));
    QCOMPARE(p.value(QStringLiteral("accent")).toString(), QStringLiteral("#89b4fa"));
}

void OmarchyThemeTest::parseMalformedYieldsDefaults()
{
    const QJsonObject empty = OmarchyTheme::parseColorsToml(QString());
    assertCanonical(empty);
    QCOMPARE(empty.value(QStringLiteral("mode")).toString(), QStringLiteral("dark"));
    QCOMPARE(empty.value(QStringLiteral("background")).toString(), QStringLiteral("#101010"));
    QCOMPARE(empty.value(QStringLiteral("foreground")).toString(), QStringLiteral("#eeeeee"));

    const QJsonObject garbage = OmarchyTheme::parseColorsToml(QStringLiteral(
        "this is not toml\n"
        "= no key\n"
        "background =\n"
        "accent = \"not-a-color\"\n"
        "[unterminated\n"
        "red = rgba(1, 2, 3, 0.4)\n"));
    assertCanonical(garbage);
    QCOMPARE(garbage.value(QStringLiteral("background")).toString(), QStringLiteral("#101010"));
    QCOMPARE(garbage.value(QStringLiteral("accent")).toString(), QStringLiteral("#5584aa"));
}

void OmarchyThemeTest::parseAlwaysCanonicalKeys()
{
    const char *fixtures[] = {kCatppuccin, kGoldRush, kLastHorizon, kFlexokiLight, ""};
    for (const char *fixture : fixtures)
        assertCanonical(OmarchyTheme::parseColorsToml(QString::fromUtf8(fixture)));
}

void OmarchyThemeTest::contrastBlackOnWhite()
{
    QCOMPARE(OmarchyTheme::contrastRatio(QStringLiteral("#000000"), QStringLiteral("#ffffff")),
             21.0);
    QCOMPARE(OmarchyTheme::contrastRatio(QStringLiteral("#ffffff"), QStringLiteral("#000000")),
             21.0);
    QCOMPARE(OmarchyTheme::contrastRatio(QStringLiteral("#000"), QStringLiteral("#fff")), 21.0);
}

void OmarchyThemeTest::contrastMidTonePairs()
{
    // #767676 = rgb(118,118,118). sRGB 118/255 = 0.462745 > 0.04045, so
    // linear = ((0.462745+0.055)/1.055)^2.4 = 0.181164. L = 0.181164.
    // Against white (L=1): (1.05) / (0.181164+0.05) = 4.5422.
    const double greyOnWhite =
        OmarchyTheme::contrastRatio(QStringLiteral("#767676"), QStringLiteral("#ffffff"));
    QVERIFY2(qAbs(greyOnWhite - 4.5422) < 0.001,
             qPrintable(QString::number(greyOnWhite, 'f', 6)));

    // Gruvbox fg/bg. #d4be98 L=0.530910, #282828 L=0.021219.
    // (0.530910+0.05) / (0.021219+0.05) = 8.1567.
    const double gruvbox =
        OmarchyTheme::contrastRatio(QStringLiteral("#d4be98"), QStringLiteral("#282828"));
    QVERIFY2(qAbs(gruvbox - 8.1567) < 0.001,
             qPrintable(QString::number(gruvbox, 'f', 6)));
}

void OmarchyThemeTest::ensureContrastAlreadySufficientUnchanged()
{
    const QString fg = QStringLiteral("#CC6666");
    QCOMPARE(OmarchyTheme::ensureContrast(fg, QStringLiteral("#000000")), fg);
    QCOMPARE(OmarchyTheme::ensureContrast(QStringLiteral("#000000"), QStringLiteral("#ffffff")),
             QStringLiteral("#000000"));
}

void OmarchyThemeTest::ensureContrastRaisesFailingPairKeepsHue()
{
    const QString fg = QStringLiteral("#cc6666");
    const QString bg = QStringLiteral("#ffffff");
    QVERIFY(OmarchyTheme::contrastRatio(fg, bg) < 4.5);

    const QString raised = OmarchyTheme::ensureContrast(fg, bg);
    QVERIFY(isRrggbb(raised));
    QVERIFY(OmarchyTheme::contrastRatio(raised, bg) >= 4.5);

    float h0 = 0, s0 = 0, l0 = 0, a0 = 1;
    float h1 = 0, s1 = 0, l1 = 0, a1 = 1;
    QColor(fg).getHslF(&h0, &s0, &l0, &a0);
    QColor(raised).getHslF(&h1, &s1, &l1, &a1);
    QVERIFY2(qAbs(h0 - h1) < 0.02f, "hue must be preserved while lightness is nudged");
    QVERIFY(l1 < l0);
}

void OmarchyThemeTest::installedThemesUserWinsSorted()
{
    TestDirs dirs;
    QVERIFY(dirs.init());
    QVERIFY(QDir().mkpath(dirs.user + QStringLiteral("/zeta")));
    QVERIFY(QDir().mkpath(dirs.user + QStringLiteral("/alpha")));
    QVERIFY(QDir().mkpath(dirs.system + QStringLiteral("/alpha")));
    QVERIFY(QDir().mkpath(dirs.system + QStringLiteral("/beta")));
    QVERIFY(QDir().mkpath(dirs.system + QStringLiteral("/gamma")));

    const QStringList names = OmarchyTheme::installedThemes();
    QCOMPARE(names, (QStringList{QStringLiteral("alpha"), QStringLiteral("beta"),
                                 QStringLiteral("gamma"), QStringLiteral("zeta")}));
}

void OmarchyThemeTest::overrideResolutionOrder()
{
    TestDirs dirs;
    QVERIFY(dirs.init());

    QVERIFY(QDir().mkpath(dirs.user + QStringLiteral("/alpha")));
    QVERIFY(QDir().mkpath(dirs.system + QStringLiteral("/alpha")));
    QVERIFY(QDir().mkpath(dirs.system + QStringLiteral("/beta")));
    QVERIFY(writeText(dirs.user + QStringLiteral("/alpha/colors.toml"),
                      "background = \"#111111\"\nforeground = \"#eeeeee\"\n"));
    QVERIFY(writeText(dirs.system + QStringLiteral("/alpha/colors.toml"),
                      "background = \"#222222\"\nforeground = \"#eeeeee\"\n"));
    QVERIFY(writeText(dirs.system + QStringLiteral("/beta/colors.toml"),
                      "background = \"#333333\"\nforeground = \"#eeeeee\"\n"));
    QVERIFY(writeText(dirs.current + QStringLiteral("/theme/colors.toml"),
                      "background = \"#444444\"\nforeground = \"#eeeeee\"\n"));
    QVERIFY(writeText(dirs.current + QStringLiteral("/theme.name"), "live-one\n"));

    OmarchyTheme theme;
    QCOMPARE(theme.name(), QStringLiteral("live-one"));
    QCOMPARE(theme.palette().value(QStringLiteral("background")).toString(),
             QStringLiteral("#444444"));

    theme.setOverrideTheme(QStringLiteral("alpha"));
    QCOMPARE(theme.overrideTheme(), QStringLiteral("alpha"));
    QCOMPARE(theme.name(), QStringLiteral("alpha"));
    QCOMPARE(theme.palette().value(QStringLiteral("background")).toString(),
             QStringLiteral("#111111"));

    theme.setOverrideTheme(QStringLiteral("beta"));
    QCOMPARE(theme.name(), QStringLiteral("beta"));
    QCOMPARE(theme.palette().value(QStringLiteral("background")).toString(),
             QStringLiteral("#333333"));

    theme.setOverrideTheme(QString());
    QCOMPARE(theme.name(), QStringLiteral("live-one"));
    QCOMPARE(theme.palette().value(QStringLiteral("background")).toString(),
             QStringLiteral("#444444"));
}

void OmarchyThemeTest::overrideUnknownFallsBackToLive()
{
    TestDirs dirs;
    QVERIFY(dirs.init());
    QVERIFY(writeText(dirs.current + QStringLiteral("/theme/colors.toml"),
                      "background = \"#444444\"\nforeground = \"#eeeeee\"\n"));
    QVERIFY(writeText(dirs.current + QStringLiteral("/theme.name"), "live-one\n"));

    OmarchyTheme theme;
    QTest::ignoreMessage(QtWarningMsg,
                         "OmarchyTheme: unknown theme 'nope', falling back to the live theme");
    theme.setOverrideTheme(QStringLiteral("nope"));
    QCOMPARE(theme.overrideTheme(), QStringLiteral("nope"));
    QCOMPARE(theme.name(), QStringLiteral("live-one"));
    QCOMPARE(theme.palette().value(QStringLiteral("background")).toString(),
             QStringLiteral("#444444"));
}

void OmarchyThemeTest::liveReloadDebouncesAndEmitsOnce()
{
    TestDirs dirs;
    QVERIFY(dirs.init());
    QVERIFY(writeText(dirs.current + QStringLiteral("/theme/colors.toml"),
                      "background = \"#111111\"\nforeground = \"#eeeeee\"\naccent = \"#0000ff\"\n"));
    QVERIFY(writeText(dirs.current + QStringLiteral("/theme.name"), "one\n"));

    OmarchyTheme theme;
    QCOMPARE(theme.palette().value(QStringLiteral("accent")).toString(), QStringLiteral("#0000ff"));

    QSignalSpy spy(&theme, &OmarchyTheme::themeChanged);
    QVERIFY(writeText(dirs.current + QStringLiteral("/theme/colors.toml"),
                      "background = \"#111111\"\nforeground = \"#eeeeee\"\naccent = \"#ff0000\"\n"));
    QVERIFY(writeText(dirs.current + QStringLiteral("/theme.name"), "two\n"));
    QVERIFY(writeText(dirs.current + QStringLiteral("/theme/colors.toml"),
                      "background = \"#111111\"\nforeground = \"#eeeeee\"\naccent = \"#00ff00\"\n"));

    QTRY_VERIFY(spy.count() >= 1);
    QTest::qWait(300);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(theme.palette().value(QStringLiteral("accent")).toString(), QStringLiteral("#00ff00"));
    QCOMPARE(theme.name(), QStringLiteral("two"));
}

void OmarchyThemeTest::backgroundImagePathFromCurrent()
{
    TestDirs dirs;
    QVERIFY(dirs.init());
    QVERIFY(writeText(dirs.current + QStringLiteral("/theme/colors.toml"),
                      "background = \"#111111\"\n"));
    QVERIFY(writeText(dirs.current + QStringLiteral("/theme.name"), "one\n"));

    OmarchyTheme missing;
    QVERIFY(missing.backgroundImagePath().isEmpty());

    const QString bg = dirs.current + QStringLiteral("/background");
    QVERIFY(writeText(bg, "not-an-image"));
    OmarchyTheme present;
    QCOMPARE(present.backgroundImagePath(), bg);
}

OMAPRESENT_TEST_SUITE(OmarchyThemeTest)
#include "tst_omarchytheme.moc"
