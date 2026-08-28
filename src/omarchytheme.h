#pragma once

// OmarchyTheme — spec §6. Reads the live Omarchy theme, supports both
// colors.toml shapes, watches for theme changes, and exposes one canonical
// palette that every surface (editor, preview, presenter, audience, PDF, web)
// consumes.
//
// Owner: the theme agent. Contract frozen.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class OmarchyTheme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY themeChanged)

public:
    explicit OmarchyTheme(QObject *parent = nullptr);
    ~OmarchyTheme() override;

    // Name of the theme in effect (the override when set, else the live one).
    QString name() const;

    // Per-deck `theme:` frontmatter override (spec §6). Looks in
    // ~/.config/omarchy/themes/<name>/ then /usr/share/omarchy/themes/<name>/.
    // Empty string returns to the live desktop theme. Never touches the
    // desktop's own theme.
    void setOverrideTheme(const QString &themeName);
    QString overrideTheme() const;

    // The canonical palette. Keys are ALWAYS present, always "#rrggbb" except
    // "mode" which is "dark" or "light", and "ansi" which is an array of 16
    // "#rrggbb" strings:
    //   mode background foreground accent muted selection
    //   red orange yellow green cyan blue magenta brown
    //   bright_red ... bright_brown
    //   dark_background dark_foreground  (fall back to muted/background)
    //   ansi[16]
    // Terminal-form colors.toml files derive the named roles from color0-15.
    QJsonObject palette() const;

    // ~/.local/state/omarchy/current/background, or empty when absent.
    // Used as the missing-image placeholder (spec §4.5 step 5).
    QString backgroundImagePath() const;

    // Themes installed under either themes directory, sorted, deduplicated.
    static QStringList installedThemes();

    // Tests only: replace the well-known directories. Empty strings restore
    // the Omarchy defaults. The application never calls this.
    static void setDirectoriesForTest(const QString &userThemes,
                                      const QString &systemThemes,
                                      const QString &currentState);

    // --- Pure helpers, directly unit-tested -------------------------------
    // Parses either colors.toml shape into the canonical palette above.
    // Never fails: unknown or missing keys get sane derived defaults.
    static QJsonObject parseColorsToml(const QString &tomlText);
    // WCAG relative-contrast ratio between two "#rrggbb" colours, 1.0 - 21.0.
    static double contrastRatio(const QString &foreground, const QString &background);
    // Spec §6 projector legibility floor: returns `foreground` unchanged when
    // it already clears `minRatio` against `background`, otherwise the same hue
    // with its lightness nudged until it does. Audience window only.
    static QString ensureContrast(const QString &foreground, const QString &background,
                                  double minRatio = 4.5);
    // Spec §6: the projector floor is for the audience window only. `audience`
    // returns a copy whose text colours have been nudged against `background`;
    // presenter, preview, pdf, web, export and editor get `palette` unchanged.
    static QJsonObject paletteForRole(const QJsonObject &palette, const QString &role,
                                      double minRatio = 4.5);

signals:
    // The live theme changed on disk, or the override moved. Every open window
    // repaints without losing its position.
    void themeChanged();

private:
    void reload();
    void watchThemeLink();

    QString m_overrideTheme;
    QString m_name;
    QJsonObject m_palette;
    QString m_backgroundImagePath;
    struct Private;
    Private *d = nullptr;
};
