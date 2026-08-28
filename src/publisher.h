#pragma once

// Publisher — spec §9. Builds the static web bundle and uploads it through a
// pluggable provider read from ~/.config/omapresent/publish.toml.
//
// Owner: the publish agent. Contract frozen.
//
// SAFETY: publishing sends the deck to an external host. Network work in this
// class must start only from an explicit publish, sign-in, or domain action.

#include <QJsonObject>
#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QStringList>

class Publisher : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit Publisher(QObject *parent = nullptr);
    ~Publisher() override;

    bool busy() const;

    // ~/.config/omapresent/publish.toml. Providers are returned as
    // { name -> { "type": "herenow|command|s3", ...provider keys } }.
    // Missing file yields the built-in anonymous herenow provider alone.
    QJsonObject providers() const;
    QString defaultProvider() const;
    Q_INVOKABLE void reloadConfig();

    // Patch exactly one key and write the file back with its comments and
    // unknown keys intact (spec §11). Never rewrites wholesale.
    Q_INVOKABLE bool setProviderKey(const QString &provider, const QString &key,
                                    const QString &value);

    // Upload an already-built bundle directory. `slug` and `access` come from
    // the deck's frontmatter (spec §4.4). Emits published() with the live URL.
    Q_INVOKABLE void publish(const QString &bundleDir, const QString &slug,
                             const QString &providerName, const QString &access);

    // Update an existing here.now Site. Other providers publish to the same
    // deterministic slug and therefore use the same operation as publish().
    Q_INVOKABLE void republish(const QString &bundleDir, const QString &slug,
                               const QString &providerName, const QString &access);

    // here.now version history and instant rollback (spec §9).
    Q_INVOKABLE void requestVersions(const QString &slug,
                                     const QString &providerName);
    Q_INVOKABLE void revert(const QString &slug, const QString &versionId,
                            const QString &providerName);

    // The here.now email code flow (spec §9), for Preferences.
    Q_INVOKABLE void requestSignInCode(const QString &email);
    Q_INVOKABLE void verifySignInCode(const QString &email, const QString &code);

    // Add a custom domain without publishing a deck first. This is an explicit
    // user action because it contacts the selected provider.
    Q_INVOKABLE bool setupDomain(const QString &domain,
                                 const QString &providerName = QString());

    // --- Pure helpers, directly unit-tested -------------------------------
    // Slug from an explicit publish.slug, else the title, else the filename:
    // lowercased, non-alphanumerics collapsed to single hyphens, trimmed.
    static QString slugify(const QString &text);
    // Minimal TOML reader for publish.toml / settings.toml. Tables become
    // nested objects. Comments and unknown keys are preserved by
    // patchToml(), not by this.
    static QJsonObject parseToml(const QString &tomlText);
    // Returns `tomlText` with one dotted key set, every comment, blank line and
    // unrelated key byte-identical. Inserts the key (and its table) when absent.
    static QString patchToml(const QString &tomlText, const QString &dottedKey,
                             const QString &value);

signals:
    void busyChanged();
    void progress(int done, int total, const QString &what);
    void published(const QString &liveUrl, const QString &slug);
    void versionsReceived(const QString &slug, const QJsonArray &versions);
    void reverted(const QString &liveUrl, const QString &slug,
                  const QString &versionId);
    void claimAvailable(const QString &claimUrl, const QString &claimToken);
    void failed(const QString &message);
    void signInCodeSent();
    void signedIn();
    void domainSetupFinished(const QString &domain, const QString &status,
                             const QJsonArray &dnsRecords);
    void domainSetupFailed(const QString &message);

private:
    struct Private;
    Private *d = nullptr;
};
