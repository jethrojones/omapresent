#pragma once

// Settings — ~/.config/omapresent/settings.toml, the editor and present-mode
// preferences documented in skill/reference/settings-toml.md.
//
// Owner: the settings agent. Contract frozen.
//
// Every getter returns the documented default when the file, the table or the
// key is absent, so the app behaves identically with no config file at all.
// set() patches exactly one key and leaves every comment, blank line and
// unknown key byte-identical (spec §11) — reuse Publisher::patchToml rather
// than writing a second TOML writer.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariant>

class Settings : public QObject {
    Q_OBJECT

public:
    explicit Settings(QObject *parent = nullptr);
    ~Settings() override;

    // Defaults to ~/.config/omapresent/settings.toml. Overridable for tests.
    void setPath(const QString &path);
    QString path() const;

    Q_INVOKABLE void reload();

    // Dotted key, e.g. "editor.dark_mode" or "presentation.inhibit_idle".
    // Returns the documented default when unset. An unknown key returns an
    // invalid QVariant — it is a programming error, not a config error.
    Q_INVOKABLE QVariant value(const QString &dottedKey) const;
    Q_INVOKABLE bool boolValue(const QString &dottedKey) const;
    Q_INVOKABLE QString stringValue(const QString &dottedKey) const;
    Q_INVOKABLE double numberValue(const QString &dottedKey) const;

    // Patches one key, preserving everything else. Creates the file and its
    // directory when absent. Returns false and leaves the file untouched on
    // any write error.
    Q_INVOKABLE bool setValue(const QString &dottedKey, const QVariant &value);

    // The whole resolved settings object, defaults merged under the file's
    // values — handy for QML bindings and for tests.
    QJsonObject resolved() const;

    // Every key this class knows, with its default. The single source of
    // truth that skill/reference/settings-toml.md is checked against.
    static QJsonObject defaults();

signals:
    // The file changed on disk, or setValue() wrote to it.
    void settingsChanged();

private:
    QString m_path;
    QJsonObject m_fileValues;
    struct Private;
    Private *d = nullptr;
};
