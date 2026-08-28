#include "publisher.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrlQuery>

#include <algorithm>

namespace {

QString publishConfigPath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("omapresent/publish.toml"));
}

QString uncommented(const QString &line) {
    bool inBasicString = false;
    bool inLiteralString = false;
    bool escaped = false;

    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar character = line.at(i);
        if (inBasicString) {
            if (escaped)
                escaped = false;
            else if (character == u'\\')
                escaped = true;
            else if (character == u'"')
                inBasicString = false;
        } else if (inLiteralString) {
            if (character == u'\'')
                inLiteralString = false;
        } else if (character == u'"') {
            inBasicString = true;
        } else if (character == u'\'') {
            inLiteralString = true;
        } else if (character == u'#') {
            return line.left(i);
        }
    }
    return line;
}

QString unquoteTomlString(const QString &text) {
    if (text.size() < 2)
        return text;
    if (text.front() == u'\'' && text.back() == u'\'')
        return text.mid(1, text.size() - 2);
    if (text.front() != u'"' || text.back() != u'"')
        return text;

    QString result;
    result.reserve(text.size() - 2);
    bool escaped = false;
    for (qsizetype i = 1; i + 1 < text.size(); ++i) {
        const QChar character = text.at(i);
        if (!escaped && character == u'\\') {
            escaped = true;
            continue;
        }
        if (escaped) {
            switch (character.unicode()) {
            case 'b': result += u'\b'; break;
            case 't': result += u'\t'; break;
            case 'n': result += u'\n'; break;
            case 'f': result += u'\f'; break;
            case 'r': result += u'\r'; break;
            case '"': result += u'"'; break;
            case '\\': result += u'\\'; break;
            default:
                result += u'\\';
                result += character;
                break;
            }
            escaped = false;
        } else {
            result += character;
        }
    }
    if (escaped)
        result += u'\\';
    return result;
}

QJsonValue parseTomlValue(const QString &source);

QStringList splitTomlArray(const QString &source) {
    QStringList values;
    qsizetype start = 0;
    int nested = 0;
    bool inBasicString = false;
    bool inLiteralString = false;
    bool escaped = false;

    for (qsizetype i = 0; i < source.size(); ++i) {
        const QChar character = source.at(i);
        if (inBasicString) {
            if (escaped)
                escaped = false;
            else if (character == u'\\')
                escaped = true;
            else if (character == u'"')
                inBasicString = false;
        } else if (inLiteralString) {
            if (character == u'\'')
                inLiteralString = false;
        } else if (character == u'"') {
            inBasicString = true;
        } else if (character == u'\'') {
            inLiteralString = true;
        } else if (character == u'[' || character == u'{') {
            ++nested;
        } else if (character == u']' || character == u'}') {
            --nested;
        } else if (character == u',' && nested == 0) {
            values.append(source.mid(start, i - start).trimmed());
            start = i + 1;
        }
    }
    const QString last = source.mid(start).trimmed();
    if (!last.isEmpty())
        values.append(last);
    return values;
}

QJsonValue parseTomlValue(const QString &source) {
    const QString value = source.trimmed();
    if ((value.startsWith(u'"') && value.endsWith(u'"'))
        || (value.startsWith(u'\'') && value.endsWith(u'\''))) {
        return unquoteTomlString(value);
    }
    if (value == QStringLiteral("true"))
        return true;
    if (value == QStringLiteral("false"))
        return false;
    if (value.startsWith(u'[') && value.endsWith(u']')) {
        QJsonArray array;
        for (const QString &item : splitTomlArray(value.mid(1, value.size() - 2)))
            array.append(parseTomlValue(item));
        return array;
    }

    QString numeric = value;
    numeric.remove(u'_');
    bool integerOk = false;
    const qlonglong integer = numeric.toLongLong(&integerOk);
    if (integerOk)
        return integer;
    bool numberOk = false;
    const double number = numeric.toDouble(&numberOk);
    if (numberOk)
        return number;
    return value;
}

QStringList dottedParts(const QString &source) {
    QStringList parts;
    for (const QString &part : source.split(u'.', Qt::SkipEmptyParts))
        parts.append(unquoteTomlString(part.trimmed()));
    return parts;
}

void setNestedValue(QJsonObject &object, const QStringList &path,
                    const QJsonValue &value) {
    if (path.isEmpty())
        return;
    if (path.size() == 1) {
        object.insert(path.constFirst(), value);
        return;
    }

    QJsonObject child = object.value(path.constFirst()).toObject();
    setNestedValue(child, path.mid(1), value);
    object.insert(path.constFirst(), child);
}

QString tomlString(const QString &value) {
    QString escaped = value;
    escaped.replace(u'\\', QStringLiteral("\\\\"));
    escaped.replace(u'"', QStringLiteral("\\\""));
    escaped.replace(u'\b', QStringLiteral("\\b"));
    escaped.replace(u'\t', QStringLiteral("\\t"));
    escaped.replace(u'\n', QStringLiteral("\\n"));
    escaped.replace(u'\f', QStringLiteral("\\f"));
    escaped.replace(u'\r', QStringLiteral("\\r"));
    return u'"' + escaped + u'"';
}

qsizetype commentStart(const QString &line) {
    bool inBasicString = false;
    bool inLiteralString = false;
    bool escaped = false;
    int arrayDepth = 0;

    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar character = line.at(i);
        if (inBasicString) {
            if (escaped)
                escaped = false;
            else if (character == u'\\')
                escaped = true;
            else if (character == u'"')
                inBasicString = false;
        } else if (inLiteralString) {
            if (character == u'\'')
                inLiteralString = false;
        } else if (character == u'"') {
            inBasicString = true;
        } else if (character == u'\'') {
            inLiteralString = true;
        } else if (character == u'[' || character == u'{') {
            ++arrayDepth;
        } else if (character == u']' || character == u'}') {
            --arrayDepth;
        } else if (character == u'#' && arrayDepth == 0) {
            return i;
        }
    }
    return -1;
}

QString valueSuffix(const QString &valueAndComment) {
    const qsizetype hash = commentStart(valueAndComment);
    const qsizetype valueEnd = hash < 0 ? valueAndComment.size() : hash;
    qsizetype suffixStart = valueEnd;
    while (suffixStart > 0 && valueAndComment.at(suffixStart - 1).isSpace())
        --suffixStart;
    return valueAndComment.mid(suffixStart);
}

struct BundleFile {
    QString path;
    QString absolutePath;
    QString contentType;
    qint64 size = 0;
    QByteArray sha256;
};

QList<BundleFile> bundleFiles(
    const QString &bundleDirectory, QString *error,
    QSharedPointer<QTemporaryDir> *snapshotDirectory) {
    const QFileInfo rootInfo(bundleDirectory);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        *error = QStringLiteral("The bundle directory does not exist: %1")
                     .arg(QDir::toNativeSeparators(bundleDirectory));
        return {};
    }

    const QDir root(rootInfo.absoluteFilePath());
    auto snapshot = QSharedPointer<QTemporaryDir>::create();
    if (!snapshot->isValid()) {
        *error = QStringLiteral("Omapresent could not make a stable copy of the bundle.");
        return {};
    }
    const QDir snapshotRoot(snapshot->path());
    const QMimeDatabase mimeDatabase;
    QList<BundleFile> files;
    QDirIterator iterator(root.absolutePath(),
                          QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString absolutePath = iterator.next();
        QFile file(absolutePath);
        if (!file.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("Cannot read bundle file %1: %2")
                         .arg(QDir::toNativeSeparators(absolutePath), file.errorString());
            return {};
        }
        BundleFile bundleFile;
        bundleFile.path = root.relativeFilePath(absolutePath).replace(u'\\', u'/');
        bundleFile.absolutePath = snapshotRoot.filePath(bundleFile.path);
        if (!QDir().mkpath(QFileInfo(bundleFile.absolutePath).absolutePath())
            || !QFile::copy(absolutePath, bundleFile.absolutePath)) {
            *error = QStringLiteral("Cannot copy bundle file %1 into the upload snapshot.")
                         .arg(bundleFile.path);
            return {};
        }
        bundleFile.size = file.size();
        bundleFile.contentType = mimeDatabase.mimeTypeForFile(
            absolutePath, QMimeDatabase::MatchExtension).name();
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file)) {
            *error = QStringLiteral("Cannot hash bundle file %1.")
                         .arg(QDir::toNativeSeparators(absolutePath));
            return {};
        }
        bundleFile.sha256 = hash.result().toHex();
        files.append(bundleFile);
    }
    std::sort(files.begin(), files.end(), [](const BundleFile &left,
                                             const BundleFile &right) {
        return left.path < right.path;
    });
    if (files.isEmpty())
        *error = QStringLiteral("The bundle directory contains no files.");
    if (error->isEmpty())
        *snapshotDirectory = snapshot;
    return files;
}

const BundleFile *findBundleFile(const QList<BundleFile> &files,
                                 const QString &path) {
    for (const BundleFile &file : files) {
        if (file.path == path)
            return &file;
    }
    return nullptr;
}

QJsonObject jsonObject(const QByteArray &data) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    return error.error == QJsonParseError::NoError && document.isObject()
        ? document.object() : QJsonObject();
}

bool replySucceeded(const QNetworkReply *reply) {
    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
}

QString replyFailure(QNetworkReply *reply, const QString &action,
                     const QByteArray &body) {
    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject response = jsonObject(body);
    QString detail = response.value(QStringLiteral("message")).toString();
    if (detail.isEmpty())
        detail = response.value(QStringLiteral("error")).toString();
    if (detail.isEmpty()) {
        const QRegularExpression xmlMessage(
            QStringLiteral("<Message>([^<]+)</Message>"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = xmlMessage.match(
            QString::fromUtf8(body));
        if (match.hasMatch())
            detail = match.captured(1).trimmed();
    }
    if (detail.isEmpty())
        detail = reply->errorString();
    QString message = QStringLiteral("%1 failed: %2").arg(action, detail);
    if (status > 0)
        message += QStringLiteral(" (HTTP %1)").arg(status);
    const int retryAfter = response.value(QStringLiteral("retry_after")).toInt();
    if (retryAfter > 0)
        message += QStringLiteral(". Try again in %1 seconds").arg(retryAfter);
    const QString docs = response.value(QStringLiteral("docs_url")).toString();
    if (!docs.isEmpty())
        message += QStringLiteral(". See %1").arg(docs);
    return message + u'.';
}

QJsonArray hereUploads(const QJsonObject &response) {
    const QJsonObject upload = response.value(QStringLiteral("upload")).toObject();
    if (!upload.isEmpty())
        return upload.value(QStringLiteral("uploads")).toArray();
    return response.value(QStringLiteral("presignedUploads")).toArray();
}

QString hereVersionId(const QJsonObject &response) {
    const QString nested = response.value(QStringLiteral("upload")).toObject()
                               .value(QStringLiteral("versionId")).toString();
    return nested.isEmpty()
        ? response.value(QStringLiteral("versionId")).toString() : nested;
}

QStringList hereSkipped(const QJsonObject &response) {
    QJsonArray skipped = response.value(QStringLiteral("upload")).toObject()
                             .value(QStringLiteral("skipped")).toArray();
    if (skipped.isEmpty())
        skipped = response.value(QStringLiteral("skipped")).toArray();
    QStringList paths;
    for (const QJsonValue &value : skipped)
        paths.append(value.toString());
    return paths;
}

QByteArray hmacSha256(QByteArray key, const QByteArray &message) {
    constexpr int blockSize = 64;
    if (key.size() > blockSize)
        key = QCryptographicHash::hash(key, QCryptographicHash::Sha256);
    key = key.leftJustified(blockSize, '\0', true);
    QByteArray outer(blockSize, '\x5c');
    QByteArray inner(blockSize, '\x36');
    for (int i = 0; i < blockSize; ++i) {
        outer[i] = outer.at(i) ^ key.at(i);
        inner[i] = inner.at(i) ^ key.at(i);
    }
    const QByteArray innerHash = QCryptographicHash::hash(
        inner + message, QCryptographicHash::Sha256);
    return QCryptographicHash::hash(outer + innerHash, QCryptographicHash::Sha256);
}

QString encodedPath(const QString &path) {
    QStringList encoded;
    for (const QString &part : path.split(u'/', Qt::KeepEmptyParts))
        encoded.append(QString::fromLatin1(QUrl::toPercentEncoding(part, "-_.~")));
    return encoded.join(u'/');
}

QString joinedUrlPath(const QStringList &parts) {
    QStringList clean;
    for (QString part : parts) {
        while (part.startsWith(u'/'))
            part.remove(0, 1);
        while (part.endsWith(u'/'))
            part.chop(1);
        if (!part.isEmpty())
            clean.append(part);
    }
    return u'/' + clean.join(u'/');
}

QString accessMode(const QString &access) {
    if (access == QStringLiteral("restricted"))
        return QStringLiteral("restricted");
    if (access == QStringLiteral("password"))
        return QStringLiteral("password");
    return QStringLiteral("anyone_with_link");
}

bool validAccess(const QString &access) {
    return access.isEmpty() || access == QStringLiteral("link")
        || access == QStringLiteral("public")
        || access == QStringLiteral("password")
        || access == QStringLiteral("restricted");
}

QString providerValue(const QJsonObject &provider, const QString &key,
                      const QString &legacyKey = {}) {
    QString value = provider.value(key).toString();
    if (value.isEmpty() && !legacyKey.isEmpty())
        value = provider.value(legacyKey).toString();
    return value;
}

QUrl hereApiBase(const QJsonObject &provider) {
    const QString configured = provider.value(QStringLiteral("api_base")).toString();
    if (configured.isEmpty())
        return QUrl(QStringLiteral("https://here.now"));

    const QUrl url(configured);
    const QHostAddress address(url.host());
    const bool loopback = url.host().compare(
                              QStringLiteral("localhost"), Qt::CaseInsensitive) == 0
        || address.isLoopback();
    if (url.scheme() != QStringLiteral("http") || !loopback
        || url.host().isEmpty() || !url.userInfo().isEmpty()) {
        return {};
    }
    return url;
}

bool validHereApiBase(const QJsonObject &provider) {
    return provider.value(QStringLiteral("api_base")).toString().isEmpty()
        || hereApiBase(provider).isValid();
}

void appendBounded(QByteArray *buffer, const QByteArray &chunk,
                   qsizetype limit = 64 * 1024) {
    buffer->append(chunk);
    const qsizetype excess = buffer->size() - limit;
    if (excess > 0)
        buffer->remove(0, excess);
}

struct HerePublishContext {
    QJsonObject provider;
    QString providerName;
    QList<BundleFile> files;
    QString requestedSlug;
    QString slug;
    QString access;
    QString siteUrl;
    QString versionId;
    QString claimToken;
    QString claimUrl;
    QSharedPointer<QTemporaryDir> snapshot;
    QJsonArray uploads;
    QSet<QString> completed;
    int uploadIndex = 0;
    int refreshes = 0;
    bool update = false;
    bool privateBootstrap = false;
    bool bootstrapComplete = false;
    bool accessBeforeUpdate = false;
};

bool isHereBootstrap(const QSharedPointer<HerePublishContext> &context) {
    return context->privateBootstrap && !context->bootstrapComplete;
}

QByteArray hereBootstrapContent() {
    return QByteArrayLiteral(
        "<!doctype html><meta charset=utf-8><title>Preparing presentation</title>");
}

int hereFileTotal(const QSharedPointer<HerePublishContext> &context) {
    return isHereBootstrap(context) ? 1 : context->files.size();
}

struct S3PublishContext {
    QJsonObject provider;
    QList<BundleFile> files;
    QString slug;
    QSharedPointer<QTemporaryDir> snapshot;
    int index = 0;
};

} // namespace

struct Publisher::Private {
    explicit Private(Publisher *publisher)
        : q(publisher), network(publisher) {}

    Publisher *q;
    QNetworkAccessManager network;
    QString configPath = publishConfigPath();
    QJsonObject config;
    QString configError;
    bool busy = false;

    bool begin();
    bool requireReadableConfig();
    void setBusy(bool value);
    void fail(const QString &message);
    void finish(const QString &liveUrl, const QString &slug);
    bool saveHereState(const QString &providerName, const QString &requestedSlug,
                       const QString &siteSlug, const QString &claimToken);
    QNetworkReply *hereRequest(const QByteArray &method, const QString &path,
                               const QJsonObject &provider,
                               const QJsonObject &body = {},
                               bool sendBody = true);
    void startHere(const QSharedPointer<HerePublishContext> &context);
    void uploadHereNext(const QSharedPointer<HerePublishContext> &context);
    void refreshHereUploads(const QSharedPointer<HerePublishContext> &context);
    void finalizeHere(const QSharedPointer<HerePublishContext> &context);
    void applyHereAccess(const QSharedPointer<HerePublishContext> &context);
    void configureHereDomain(const QSharedPointer<HerePublishContext> &context);
    void configureHereMount(const QSharedPointer<HerePublishContext> &context);
    void startCommand(const QJsonObject &provider, const QList<BundleFile> &files,
                      const QSharedPointer<QTemporaryDir> &snapshot,
                      const QString &slug);
    void startS3(const QSharedPointer<S3PublishContext> &context);
};

bool Publisher::Private::begin() {
    if (busy) {
        emit q->failed(QStringLiteral(
            "Another publish operation is still running. Wait for it to finish."));
        return false;
    }
    setBusy(true);
    return true;
}

bool Publisher::Private::requireReadableConfig() {
    if (configError.isEmpty())
        return true;
    emit q->failed(configError);
    return false;
}

void Publisher::Private::setBusy(bool value) {
    if (busy == value)
        return;
    busy = value;
    emit q->busyChanged();
}

void Publisher::Private::fail(const QString &message) {
    setBusy(false);
    emit q->failed(message);
}

void Publisher::Private::finish(const QString &liveUrl, const QString &slug) {
    setBusy(false);
    emit q->published(liveUrl, slug);
}

bool Publisher::Private::saveHereState(const QString &providerName,
                                       const QString &requestedSlug,
                                       const QString &siteSlug,
                                       const QString &claimToken) {
    QFile input(configPath);
    QString original;
    if (input.exists()) {
        if (!input.open(QIODevice::ReadOnly))
            return false;
        original = QString::fromUtf8(input.readAll());
    }
    if (!QDir().mkpath(QFileInfo(configPath).absolutePath()))
        return false;
    QSaveFile output(configPath);
    if (!output.open(QIODevice::WriteOnly))
        return false;
    QString patched = Publisher::patchToml(
        original,
        QStringLiteral("providers.%1.sites.%2").arg(providerName, requestedSlug),
        siteSlug);
    if (!claimToken.isEmpty()) {
        patched = Publisher::patchToml(
            patched,
            QStringLiteral("providers.%1.claim_tokens.%2")
                .arg(providerName, siteSlug),
            claimToken);
    }
    if (output.write(patched.toUtf8()) < 0 || !output.commit())
        return false;
    q->reloadConfig();
    return true;
}

QNetworkReply *Publisher::Private::hereRequest(const QByteArray &method,
                                                const QString &path,
                                                const QJsonObject &provider,
                                                const QJsonObject &body,
                                                bool sendBody) {
    QNetworkRequest request(hereApiBase(provider).resolved(QUrl(path)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Omapresent/1.0"));
    const QString apiKey = provider.value(QStringLiteral("api_key")).toString();
    if (!apiKey.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + apiKey.toUtf8());
    QByteArray payload;
    if (sendBody) {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/json"));
        payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    }
    return network.sendCustomRequest(request, method, payload);
}

void Publisher::Private::startHere(
    const QSharedPointer<HerePublishContext> &context) {
    QJsonArray files;
    if (isHereBootstrap(context)) {
        const QByteArray content = hereBootstrapContent();
        files.append(QJsonObject{
            {QStringLiteral("path"), QStringLiteral("index.html")},
            {QStringLiteral("size"), content.size()},
            {QStringLiteral("contentType"), QStringLiteral("text/html; charset=utf-8")},
            {QStringLiteral("hash"), QString::fromLatin1(
                 QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex())},
        });
    } else {
        for (const BundleFile &file : context->files) {
            files.append(QJsonObject{
                {QStringLiteral("path"), file.path},
                {QStringLiteral("size"), file.size},
                {QStringLiteral("contentType"), file.contentType},
                {QStringLiteral("hash"), QString::fromLatin1(file.sha256)},
            });
        }
    }
    QJsonObject body{{QStringLiteral("files"), files}};
    if (context->update) {
        context->claimToken = context->provider
                                  .value(QStringLiteral("claim_token")).toString();
        if (!context->claimToken.isEmpty())
            body.insert(QStringLiteral("claimToken"), context->claimToken);
    }

    const QString path = context->update
        ? QStringLiteral("/api/v1/publish/%1").arg(context->slug)
        : QStringLiteral("/api/v1/publish");
    const QString targetSlug = context->slug;
    QNetworkReply *reply = hereRequest(
        context->update ? QByteArrayLiteral("PUT") : QByteArrayLiteral("POST"),
        path, context->provider, body);
    QObject::connect(reply, &QNetworkReply::finished, q,
                     [this, context, reply, targetSlug] {
        const QByteArray responseBody = reply->readAll();
        if (!replySucceeded(reply)) {
            const QString message = replyFailure(
                reply, context->update ? QStringLiteral("here.now republish")
                                       : QStringLiteral("here.now publish"),
                responseBody);
            reply->deleteLater();
            fail(message);
            return;
        }
        const QJsonObject response = jsonObject(responseBody);
        context->slug = response.value(QStringLiteral("slug")).toString();
        if (context->slug.isEmpty() && context->update)
            context->slug = targetSlug;
        context->siteUrl = response.value(QStringLiteral("siteUrl")).toString();
        context->versionId = hereVersionId(response);
        const QString returnedClaimToken = response
                                               .value(QStringLiteral("claimToken"))
                                               .toString();
        if (!returnedClaimToken.isEmpty())
            context->claimToken = returnedClaimToken;
        const QString returnedClaimUrl = response
                                             .value(QStringLiteral("claimUrl"))
                                             .toString();
        if (!returnedClaimUrl.isEmpty())
            context->claimUrl = returnedClaimUrl;
        context->uploads = hereUploads(response);
        for (const QString &path : hereSkipped(response)) {
            if (!context->completed.contains(path)) {
                context->completed.insert(path);
                if (!isHereBootstrap(context)) {
                    emit q->progress(context->completed.size(), context->files.size(),
                                     QStringLiteral("Unchanged: %1").arg(path));
                }
            }
        }
        reply->deleteLater();

        if (context->slug.isEmpty() || context->versionId.isEmpty()) {
            fail(QStringLiteral(
                "here.now returned an incomplete publish response. Try again; "
                "if this continues, check https://here.now/docs."));
            return;
        }
        if (!context->update) {
            if (!saveHereState(context->providerName, context->requestedSlug,
                               context->slug, context->claimToken)) {
                if (!context->claimToken.isEmpty())
                    emit q->claimAvailable(context->claimUrl, context->claimToken);
                fail(QStringLiteral(
                    "Omapresent could not save the here.now Site details to %1. "
                    "Fix that file's permissions, then publish again.")
                         .arg(configPath));
                return;
            }
        }
        if (!context->claimToken.isEmpty()) {
            emit q->claimAvailable(context->claimUrl, context->claimToken);
        }
        uploadHereNext(context);
    });
}

void Publisher::Private::uploadHereNext(
    const QSharedPointer<HerePublishContext> &context) {
    while (context->uploadIndex < context->uploads.size()) {
        const QJsonObject upload = context->uploads.at(context->uploadIndex).toObject();
        if (!context->completed.contains(upload.value(QStringLiteral("path")).toString()))
            break;
        ++context->uploadIndex;
    }
    if (context->uploadIndex >= context->uploads.size()) {
        if (context->completed.size() < hereFileTotal(context)) {
            QStringList missing;
            if (isHereBootstrap(context)) {
                missing.append(QStringLiteral("the private-deck placeholder"));
            } else {
                for (const BundleFile &file : context->files) {
                    if (!context->completed.contains(file.path))
                        missing.append(file.path);
                }
            }
            fail(QStringLiteral("here.now did not provide upload URLs for: %1.")
                     .arg(missing.join(QStringLiteral(", "))));
            return;
        }
        finalizeHere(context);
        return;
    }

    const QJsonObject upload = context->uploads.at(context->uploadIndex).toObject();
    const QString path = upload.value(QStringLiteral("path")).toString();
    const BundleFile *bundleFile = findBundleFile(context->files, path);
    const QUrl uploadUrl(upload.value(QStringLiteral("url")).toString());
    const bool bootstrapFile = isHereBootstrap(context)
        && path == QStringLiteral("index.html");
    if ((!bundleFile && !bootstrapFile) || !uploadUrl.isValid()
        || uploadUrl.scheme().isEmpty()) {
        fail(QStringLiteral("here.now returned an invalid upload target for %1.")
                 .arg(path.isEmpty() ? QStringLiteral("a bundle file") : path));
        return;
    }

    QIODevice *uploadData = nullptr;
    QString contentType;
    if (bootstrapFile) {
        QBuffer *buffer = new QBuffer(q);
        buffer->setData(hereBootstrapContent());
        buffer->open(QIODevice::ReadOnly);
        uploadData = buffer;
        contentType = QStringLiteral("text/html; charset=utf-8");
    } else {
        QFile *file = new QFile(bundleFile->absolutePath, q);
        if (!file->open(QIODevice::ReadOnly)) {
            const QString message = QStringLiteral("Cannot read %1 for upload: %2")
                                        .arg(path, file->errorString());
            file->deleteLater();
            fail(message);
            return;
        }
        uploadData = file;
        contentType = bundleFile->contentType;
    }
    QNetworkRequest request(uploadUrl);
    const QJsonObject headers = upload.value(QStringLiteral("headers")).toObject();
    for (auto iterator = headers.constBegin(); iterator != headers.constEnd(); ++iterator)
        request.setRawHeader(iterator.key().toUtf8(), iterator.value().toString().toUtf8());
    if (!request.hasRawHeader("Content-Type"))
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    QNetworkReply *reply = network.put(request, uploadData);
    uploadData->setParent(reply);
    QObject::connect(reply, &QNetworkReply::finished, q,
                     [this, context, reply, path] {
        const QByteArray responseBody = reply->readAll();
        if (!replySucceeded(reply)) {
            const int status = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray lowerBody = responseBody.toLower();
            const bool expired = status == 401 || status == 403
                || reply->error() == QNetworkReply::TimeoutError
                || lowerBody.contains("expired");
            if (expired && context->refreshes < 5) {
                reply->deleteLater();
                refreshHereUploads(context);
                return;
            }
            const QString message = replyFailure(
                reply, QStringLiteral("Upload of %1").arg(path), responseBody);
            reply->deleteLater();
            fail(message);
            return;
        }
        reply->deleteLater();
        context->completed.insert(path);
        if (!isHereBootstrap(context))
            emit q->progress(context->completed.size(), context->files.size(), path);
        ++context->uploadIndex;
        uploadHereNext(context);
    });
}

void Publisher::Private::refreshHereUploads(
    const QSharedPointer<HerePublishContext> &context) {
    ++context->refreshes;
    QJsonObject body;
    if (!context->claimToken.isEmpty())
        body.insert(QStringLiteral("claimToken"), context->claimToken);
    QNetworkReply *reply = hereRequest(
        QByteArrayLiteral("POST"),
        QStringLiteral("/api/v1/publish/%1/uploads/refresh").arg(context->slug),
        context->provider, body);
    QObject::connect(reply, &QNetworkReply::finished, q,
                     [this, context, reply] {
        const QByteArray responseBody = reply->readAll();
        if (!replySucceeded(reply)) {
            const QString message = replyFailure(
                reply, QStringLiteral("Refresh of expired upload URLs"), responseBody);
            reply->deleteLater();
            fail(message);
            return;
        }
        const QJsonObject response = jsonObject(responseBody);
        context->uploads = hereUploads(response);
        for (const QString &path : hereSkipped(response)) {
            if (!context->completed.contains(path)) {
                context->completed.insert(path);
                if (!isHereBootstrap(context)) {
                    emit q->progress(context->completed.size(), context->files.size(),
                                     QStringLiteral("Unchanged: %1").arg(path));
                }
            }
        }
        context->uploadIndex = 0;
        reply->deleteLater();
        uploadHereNext(context);
    });
}

void Publisher::Private::finalizeHere(
    const QSharedPointer<HerePublishContext> &context) {
    QNetworkReply *reply = hereRequest(
        QByteArrayLiteral("POST"),
        QStringLiteral("/api/v1/publish/%1/finalize").arg(context->slug),
        context->provider,
        QJsonObject{{QStringLiteral("versionId"), context->versionId}});
    QObject::connect(reply, &QNetworkReply::finished, q,
                     [this, context, reply] {
        const QByteArray responseBody = reply->readAll();
        if (!replySucceeded(reply)) {
            const QString message = replyFailure(
                reply, QStringLiteral("Finalizing the here.now Site"), responseBody);
            reply->deleteLater();
            fail(message);
            return;
        }
        const QString siteUrl = jsonObject(responseBody)
                                    .value(QStringLiteral("siteUrl")).toString();
        if (!siteUrl.isEmpty())
            context->siteUrl = siteUrl;
        reply->deleteLater();
        if (isHereBootstrap(context))
            applyHereAccess(context);
        else if (context->privateBootstrap)
            configureHereDomain(context);
        else
            applyHereAccess(context);
    });
}

void Publisher::Private::applyHereAccess(
    const QSharedPointer<HerePublishContext> &context) {
    const QString mode = accessMode(context->access);
    const QString apiKey = context->provider.value(QStringLiteral("api_key")).toString();
    if (apiKey.isEmpty()) {
        configureHereDomain(context);
        return;
    }

    QNetworkReply *reply = nullptr;
    if (mode == QStringLiteral("password")) {
        const QString password = context->provider
                                     .value(QStringLiteral("password")).toString();
        reply = hereRequest(
            QByteArrayLiteral("PATCH"),
            QStringLiteral("/api/v1/publish/%1/metadata").arg(context->slug),
            context->provider,
            QJsonObject{{QStringLiteral("password"), password}});
    } else {
        QJsonObject body{{QStringLiteral("mode"), mode}};
        if (mode == QStringLiteral("restricted")) {
            body.insert(QStringLiteral("allowedEmails"),
                        context->provider.value(QStringLiteral("allowed_emails")));
            body.insert(QStringLiteral("allowedDomains"),
                        context->provider.value(QStringLiteral("allowed_domains")));
            body.insert(QStringLiteral("notify"), false);
        }
        reply = hereRequest(
            QByteArrayLiteral("PATCH"),
            QStringLiteral("/api/v1/publish/%1/access").arg(context->slug),
            context->provider, body);
    }
    QObject::connect(reply, &QNetworkReply::finished, q,
                     [this, context, reply] {
        const QByteArray responseBody = reply->readAll();
        if (!replySucceeded(reply)) {
            const QString message = replyFailure(
                reply, QStringLiteral("Setting published access"), responseBody);
            reply->deleteLater();
            fail(message);
            return;
        }
        reply->deleteLater();
        if (context->accessBeforeUpdate) {
            context->accessBeforeUpdate = false;
            startHere(context);
        } else if (isHereBootstrap(context)) {
            context->bootstrapComplete = true;
            context->update = true;
            context->completed.clear();
            context->uploads = {};
            context->uploadIndex = 0;
            context->refreshes = 0;
            context->versionId.clear();
            startHere(context);
        } else {
            configureHereDomain(context);
        }
    });
}

void Publisher::Private::configureHereDomain(
    const QSharedPointer<HerePublishContext> &context) {
    const QString domain = context->provider.value(QStringLiteral("domain")).toString();
    if (domain.isEmpty()) {
        finish(context->siteUrl, context->slug);
        return;
    }
    QNetworkReply *reply = hereRequest(
        QByteArrayLiteral("POST"), QStringLiteral("/api/v1/domains"),
        context->provider, QJsonObject{{QStringLiteral("domain"), domain}});
    QObject::connect(reply, &QNetworkReply::finished, q,
                     [this, context, reply] {
        const QByteArray responseBody = reply->readAll();
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (!replySucceeded(reply) && status != 409) {
            const QString message = replyFailure(
                reply, QStringLiteral("Adding the custom domain"), responseBody);
            reply->deleteLater();
            fail(message);
            return;
        }
        reply->deleteLater();
        configureHereMount(context);
    });
}

void Publisher::Private::configureHereMount(
    const QSharedPointer<HerePublishContext> &context) {
    const QString domain = context->provider.value(QStringLiteral("domain")).toString();
    QString prefix = context->provider
                         .value(QStringLiteral("mount_prefix")).toString();
    if (prefix.isEmpty())
        prefix = QStringLiteral("/presentations");
    const QString mountPath = joinedUrlPath({prefix, context->requestedSlug});
    const QJsonObject body{
        {QStringLiteral("domain"), domain},
        {QStringLiteral("mount_path"), mountPath},
        {QStringLiteral("slug"), context->slug},
    };
    QNetworkReply *reply = hereRequest(
        QByteArrayLiteral("POST"), QStringLiteral("/api/v1/mounts"),
        context->provider, body);
    QObject::connect(reply, &QNetworkReply::finished, q,
                     [this, context, reply, domain, mountPath] {
        const QByteArray responseBody = reply->readAll();
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (replySucceeded(reply)) {
            QString url = jsonObject(responseBody).value(QStringLiteral("url")).toString();
            if (url.isEmpty())
                url = QStringLiteral("https://%1%2").arg(domain, mountPath);
            reply->deleteLater();
            finish(url, context->slug);
            return;
        }
        if (status != 409) {
            const QString message = replyFailure(
                reply, QStringLiteral("Mounting the Site on the custom domain"),
                responseBody);
            reply->deleteLater();
            fail(message);
            return;
        }
        reply->deleteLater();

        const QString encodedMount = QString::fromLatin1(
            QUrl::toPercentEncoding(mountPath, QByteArrayLiteral("-_.~")));
        QNetworkReply *patchReply = hereRequest(
            QByteArrayLiteral("PATCH"),
            QStringLiteral("/api/v1/mounts/%1").arg(encodedMount),
            context->provider,
            QJsonObject{{QStringLiteral("domain"), domain},
                        {QStringLiteral("slug"), context->slug}});
        QObject::connect(patchReply, &QNetworkReply::finished, q,
                         [this, context, patchReply, domain, mountPath] {
            const QByteArray patchBody = patchReply->readAll();
            if (!replySucceeded(patchReply)) {
                const QString message = replyFailure(
                    patchReply, QStringLiteral("Updating the custom-domain mount"),
                    patchBody);
                patchReply->deleteLater();
                fail(message);
                return;
            }
            QString url = jsonObject(patchBody).value(QStringLiteral("url")).toString();
            if (url.isEmpty())
                url = QStringLiteral("https://%1%2").arg(domain, mountPath);
            patchReply->deleteLater();
            finish(url, context->slug);
        });
    });
}

void Publisher::Private::startCommand(const QJsonObject &provider,
                                       const QList<BundleFile> &files,
                                       const QSharedPointer<QTemporaryDir> &snapshot,
                                       const QString &slug) {
    const QString command = provider.value(QStringLiteral("publish")).toString();
    QProcess *process = new QProcess(q);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("OMAPRESENT_BUNDLE"),
                       snapshot->path());
    environment.insert(QStringLiteral("OMAPRESENT_SLUG"), slug);
    process->setProcessEnvironment(environment);
    process->setWorkingDirectory(snapshot->path());
    process->setProcessChannelMode(QProcess::SeparateChannels);
    auto standardOutput = QSharedPointer<QByteArray>::create();
    auto standardError = QSharedPointer<QByteArray>::create();
    QObject::connect(process, &QProcess::readyReadStandardOutput, q,
                     [process, standardOutput] {
        appendBounded(standardOutput.data(), process->readAllStandardOutput());
    });
    QObject::connect(process, &QProcess::readyReadStandardError, q,
                     [process, standardError] {
        appendBounded(standardError.data(), process->readAllStandardError());
    });
    QObject::connect(process, &QProcess::errorOccurred, q,
                     [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        const QString detail = process->errorString();
        process->deleteLater();
        fail(QStringLiteral("The configured publish command could not start: %1.")
                 .arg(detail));
    });
    QObject::connect(process,
                     qOverload<int, QProcess::ExitStatus>(&QProcess::finished), q,
                     [this, process, files, snapshot, slug, standardOutput,
                      standardError](
                         int exitCode, QProcess::ExitStatus status) {
        appendBounded(standardOutput.data(), process->readAllStandardOutput());
        appendBounded(standardError.data(), process->readAllStandardError());
        const QString errorText = QString::fromUtf8(*standardError).trimmed();
        const QString outputText = QString::fromUtf8(*standardOutput);
        process->deleteLater();
        if (status != QProcess::NormalExit || exitCode != 0) {
            const QString detail = errorText.isEmpty()
                ? QStringLiteral("It exited with code %1").arg(exitCode)
                : errorText;
            fail(QStringLiteral("The configured publish command failed: %1.")
                     .arg(detail));
            return;
        }
        QString liveUrl;
        const QStringList lines = outputText.split(
            QRegularExpression(QStringLiteral("\\r?\\n")));
        for (auto iterator = lines.crbegin(); iterator != lines.crend(); ++iterator) {
            if (!iterator->trimmed().isEmpty()) {
                liveUrl = iterator->trimmed();
                break;
            }
        }
        const QUrl parsed(liveUrl);
        if (!parsed.isValid()
            || (parsed.scheme() != QStringLiteral("https")
                && parsed.scheme() != QStringLiteral("http"))) {
            fail(QStringLiteral(
                "The publish command succeeded but its last output line was not "
                "an HTTP URL."));
            return;
        }
        int done = 0;
        for (const BundleFile &file : files)
            emit q->progress(++done, files.size(), file.path);
        finish(liveUrl, slug);
    });
    process->start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), command});
}

void Publisher::Private::startS3(
    const QSharedPointer<S3PublishContext> &context) {
    if (context->index >= context->files.size()) {
        QString baseUrl = context->provider
                              .value(QStringLiteral("base_url")).toString();
        while (baseUrl.endsWith(u'/'))
            baseUrl.chop(1);
        const QString prefix = context->provider
                                   .value(QStringLiteral("prefix")).toString();
        finish(baseUrl + joinedUrlPath(
                   {prefix, context->slug, QStringLiteral("index.html")}),
               context->slug);
        return;
    }

    const BundleFile &file = context->files.at(context->index);
    const QString configuredAccessKey = providerValue(
        context->provider, QStringLiteral("access_key_id"),
        QStringLiteral("access_key"));
    const QString accessKey = configuredAccessKey.isEmpty()
        ? QString::fromUtf8(qgetenv("AWS_ACCESS_KEY_ID"))
        : configuredAccessKey;
    const QString configuredSecretKey = providerValue(
        context->provider, QStringLiteral("secret_access_key"),
        QStringLiteral("secret_key"));
    const QString secretKey = configuredSecretKey.isEmpty()
        ? QString::fromUtf8(qgetenv("AWS_SECRET_ACCESS_KEY"))
        : configuredSecretKey;
    QString sessionToken = context->provider
                               .value(QStringLiteral("session_token")).toString();
    if (sessionToken.isEmpty())
        sessionToken = QString::fromUtf8(qgetenv("AWS_SESSION_TOKEN"));
    QString region = context->provider.value(QStringLiteral("region")).toString();
    const QUrl endpoint(context->provider.value(QStringLiteral("endpoint")).toString());
    if (region.isEmpty())
        region = QString::fromUtf8(qgetenv("AWS_REGION"));
    if (region.isEmpty()) {
        const QStringList hostParts = endpoint.host().split(u'.');
        const int s3Part = hostParts.indexOf(QStringLiteral("s3"));
        if (s3Part >= 0 && s3Part + 1 < hostParts.size()
            && hostParts.at(s3Part + 1) != QStringLiteral("amazonaws")) {
            region = hostParts.at(s3Part + 1);
        }
    }
    if (region.isEmpty())
        region = QStringLiteral("us-east-1");

    const QString bucket = context->provider.value(QStringLiteral("bucket")).toString();
    const QString prefix = context->provider.value(QStringLiteral("prefix")).toString();
    const QString objectPath = joinedUrlPath(
        {endpoint.path(), bucket, prefix, context->slug, file.path});
    QUrl uploadUrl = endpoint;
    uploadUrl.setPath(objectPath);
    uploadUrl.setQuery(QString());

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QByteArray date = now.toString(QStringLiteral("yyyyMMdd")).toLatin1();
    const QByteArray timestamp = now.toString(
        QStringLiteral("yyyyMMdd'T'HHmmss'Z'")).toLatin1();
    QByteArray host = endpoint.host().toUtf8();
    if (endpoint.port() > 0)
        host += ':' + QByteArray::number(endpoint.port());
    const QByteArray contentType = file.contentType.toUtf8();
    const QByteArray payloadHash = file.sha256;
    QByteArray canonicalHeaders = "content-type:" + contentType + '\n'
        + "host:" + host + '\n'
        + "x-amz-content-sha256:" + payloadHash + '\n'
        + "x-amz-date:" + timestamp + '\n';
    QByteArray signedHeaders = "content-type;host;x-amz-content-sha256;x-amz-date";
    if (!sessionToken.isEmpty()) {
        canonicalHeaders += "x-amz-security-token:" + sessionToken.toUtf8() + '\n';
        signedHeaders += ";x-amz-security-token";
    }
    const QByteArray canonicalRequest = "PUT\n"
        + encodedPath(objectPath).toUtf8() + "\n\n" + canonicalHeaders + '\n'
        + signedHeaders + '\n' + payloadHash;
    const QByteArray scope = date + '/' + region.toUtf8() + "/s3/aws4_request";
    const QByteArray stringToSign = "AWS4-HMAC-SHA256\n" + timestamp + '\n'
        + scope + '\n'
        + QCryptographicHash::hash(canonicalRequest,
                                   QCryptographicHash::Sha256).toHex();
    const QByteArray dateKey = hmacSha256("AWS4" + secretKey.toUtf8(), date);
    const QByteArray regionKey = hmacSha256(dateKey, region.toUtf8());
    const QByteArray serviceKey = hmacSha256(regionKey, QByteArrayLiteral("s3"));
    const QByteArray signingKey = hmacSha256(serviceKey,
                                              QByteArrayLiteral("aws4_request"));
    const QByteArray signature = hmacSha256(signingKey, stringToSign).toHex();
    const QByteArray authorization = "AWS4-HMAC-SHA256 Credential="
        + accessKey.toUtf8() + '/' + scope + ", SignedHeaders=" + signedHeaders
        + ", Signature=" + signature;

    QFile *uploadFile = new QFile(file.absolutePath, q);
    if (!uploadFile->open(QIODevice::ReadOnly)) {
        const QString message = QStringLiteral("Cannot read %1 for S3 upload: %2")
                                    .arg(file.path, uploadFile->errorString());
        uploadFile->deleteLater();
        fail(message);
        return;
    }
    QNetworkRequest request(uploadUrl);
    request.setRawHeader("Content-Type", contentType);
    request.setRawHeader("Host", host);
    request.setRawHeader("x-amz-content-sha256", payloadHash);
    request.setRawHeader("x-amz-date", timestamp);
    if (!sessionToken.isEmpty())
        request.setRawHeader("x-amz-security-token", sessionToken.toUtf8());
    request.setRawHeader("Authorization", authorization);
    QNetworkReply *reply = network.put(request, uploadFile);
    uploadFile->setParent(reply);
    QObject::connect(reply, &QNetworkReply::finished, q,
                     [this, context, reply, path = file.path] {
        const QByteArray responseBody = reply->readAll();
        if (!replySucceeded(reply)) {
            const QString message = replyFailure(
                reply, QStringLiteral("S3 upload of %1").arg(path), responseBody);
            reply->deleteLater();
            fail(message);
            return;
        }
        reply->deleteLater();
        ++context->index;
        emit q->progress(context->index, context->files.size(), path);
        startS3(context);
    });
}

Publisher::Publisher(QObject *parent)
    : QObject(parent), d(new Private(this)) {
    reloadConfig();
}

Publisher::~Publisher() {
    delete d;
}

bool Publisher::busy() const {
    return d->busy;
}

QJsonObject Publisher::providers() const {
    QJsonObject result = d->config.value(QStringLiteral("providers")).toObject();
    QJsonObject herenow = result.value(QStringLiteral("herenow")).toObject();
    if (herenow.value(QStringLiteral("type")).toString().isEmpty())
        herenow.insert(QStringLiteral("type"), QStringLiteral("herenow"));
    result.insert(QStringLiteral("herenow"), herenow);
    return result;
}

QString Publisher::defaultProvider() const {
    const QString configured = d->config.value(QStringLiteral("default")).toString();
    return configured.isEmpty() ? QStringLiteral("herenow") : configured;
}

void Publisher::reloadConfig() {
    QFile file(d->configPath);
    if (!file.exists()) {
        d->config = {};
        d->configError.clear();
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        d->config = {};
        d->configError = QStringLiteral(
            "Cannot read the existing publish config at %1: %2. Fix the file before publishing.")
                             .arg(d->configPath, file.errorString());
        return;
    }
    const QByteArray contents = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        d->config = {};
        d->configError = QStringLiteral(
            "Cannot read the existing publish config at %1: %2. Fix the file before publishing.")
                             .arg(d->configPath, file.errorString());
        return;
    }
    d->config = parseToml(QString::fromUtf8(contents));
    d->configError.clear();
}

bool Publisher::setProviderKey(const QString &provider, const QString &key,
                               const QString &value) {
    if (provider.trimmed().isEmpty() || key.trimmed().isEmpty()
        || provider.contains(u'.') || key.contains(u'.')
        || provider.contains(u'\n') || key.contains(u'\n')) {
        return false;
    }

    QFile input(d->configPath);
    QString original;
    if (input.exists()) {
        if (!input.open(QIODevice::ReadOnly))
            return false;
        original = QString::fromUtf8(input.readAll());
    }

    const QFileInfo configInfo(d->configPath);
    if (!QDir().mkpath(configInfo.absolutePath()))
        return false;

    QSaveFile output(d->configPath);
    if (!output.open(QIODevice::WriteOnly))
        return false;
    const QString patched = patchToml(
        original, QStringLiteral("providers.%1.%2").arg(provider, key), value);
    if (output.write(patched.toUtf8()) < 0 || !output.commit())
        return false;
    reloadConfig();
    return true;
}

void Publisher::publish(const QString &bundleDir, const QString &slug,
                        const QString &providerName, const QString &access) {
    reloadConfig();
    if (!d->requireReadableConfig())
        return;
    const QString selected = providerName.isEmpty() ? defaultProvider() : providerName;
    const QJsonObject provider = providers().value(selected).toObject();
    if (provider.isEmpty()) {
        emit failed(QStringLiteral("Publish provider '%1' is not configured.")
                        .arg(selected));
        return;
    }
    if (!validAccess(access)) {
        emit failed(QStringLiteral(
            "Publish access '%1' is invalid. Use link, public, password, or restricted.")
                        .arg(access));
        return;
    }

    const QString safeSlug = slugify(slug);
    QString type = provider.value(QStringLiteral("type")).toString();
    if (type.isEmpty() && selected == QStringLiteral("herenow"))
        type = QStringLiteral("herenow");
    if (type != QStringLiteral("herenow") && type != QStringLiteral("command")
        && type != QStringLiteral("s3")) {
        emit failed(QStringLiteral("Publish provider '%1' has unknown type '%2'.")
                        .arg(selected, type));
        return;
    }
    if (type != QStringLiteral("herenow")
        && accessMode(access) != QStringLiteral("anyone_with_link")) {
        emit failed(QStringLiteral(
            "Publish provider '%1' cannot enforce password or restricted access. "
            "Use a signed-in here.now provider for this deck.")
                        .arg(selected));
        return;
    }
    if (type == QStringLiteral("command")
        && provider.value(QStringLiteral("publish")).toString().isEmpty()) {
        emit failed(QStringLiteral(
            "The command provider '%1' has no publish command.").arg(selected));
        return;
    }
    if (type == QStringLiteral("herenow")) {
        if (!validHereApiBase(provider)) {
            emit failed(QStringLiteral(
                "The here.now api_base override must be an HTTP loopback URL."));
            return;
        }
        const QString apiKey = provider.value(QStringLiteral("api_key")).toString();
        if ((access == QStringLiteral("password")
             || access == QStringLiteral("restricted"))
            && apiKey.isEmpty()) {
            emit failed(QStringLiteral(
                "Sign in to here.now before publishing a password or restricted deck."));
            return;
        }
        if (access == QStringLiteral("password")
            && provider.value(QStringLiteral("password")).toString().isEmpty()) {
            emit failed(QStringLiteral(
                "Set providers.%1.password before publishing with password access.")
                            .arg(selected));
            return;
        }
        if (!provider.value(QStringLiteral("domain")).toString().isEmpty()
            && apiKey.isEmpty()) {
            emit failed(QStringLiteral(
                "Sign in to here.now before publishing to a custom domain."));
            return;
        }
    }
    if (type == QStringLiteral("s3")) {
        const QString configuredAccessKey = providerValue(
            provider, QStringLiteral("access_key_id"), QStringLiteral("access_key"));
        const QString accessKey = configuredAccessKey.isEmpty()
            ? QString::fromUtf8(qgetenv("AWS_ACCESS_KEY_ID"))
            : configuredAccessKey;
        const QString configuredSecretKey = providerValue(
            provider, QStringLiteral("secret_access_key"),
            QStringLiteral("secret_key"));
        const QString secretKey = configuredSecretKey.isEmpty()
            ? QString::fromUtf8(qgetenv("AWS_SECRET_ACCESS_KEY"))
            : configuredSecretKey;
        const QUrl endpoint(provider.value(QStringLiteral("endpoint")).toString());
        const QString bucket = provider.value(QStringLiteral("bucket")).toString();
        const QUrl baseUrl(provider.value(QStringLiteral("base_url")).toString());
        if (!endpoint.isValid() || endpoint.host().isEmpty()
            || bucket.isEmpty() || !baseUrl.isValid() || baseUrl.host().isEmpty()) {
            emit failed(QStringLiteral(
                "The S3 provider needs valid endpoint, bucket, and base_url values."));
            return;
        }
        if (accessKey.isEmpty() || secretKey.isEmpty()) {
            emit failed(QStringLiteral(
                "The S3 provider needs access_key_id and secret_access_key, or "
                "the standard AWS credential environment variables."));
            return;
        }
    }
    QString error;
    QSharedPointer<QTemporaryDir> snapshot;
    const QList<BundleFile> files = bundleFiles(bundleDir, &error, &snapshot);
    if (!error.isEmpty()) {
        emit failed(error);
        return;
    }
    if (!d->begin())
        return;

    if (type == QStringLiteral("command")) {
        d->startCommand(provider, files, snapshot, safeSlug);
    } else if (type == QStringLiteral("s3")) {
        auto context = QSharedPointer<S3PublishContext>::create();
        context->provider = provider;
        context->files = files;
        context->slug = safeSlug;
        context->snapshot = snapshot;
        d->startS3(context);
    } else {
        auto context = QSharedPointer<HerePublishContext>::create();
        context->provider = provider;
        context->providerName = selected;
        context->files = files;
        context->snapshot = snapshot;
        context->requestedSlug = safeSlug;
        context->access = access;
        context->privateBootstrap = accessMode(access) != QStringLiteral("anyone_with_link");
        d->startHere(context);
    }
}

void Publisher::republish(const QString &bundleDir, const QString &slug,
                          const QString &providerName, const QString &access) {
    reloadConfig();
    if (!d->requireReadableConfig())
        return;
    if (!validAccess(access)) {
        emit failed(QStringLiteral(
            "Publish access '%1' is invalid. Use link, public, password, or restricted.")
                        .arg(access));
        return;
    }
    const QString selected = providerName.isEmpty() ? defaultProvider() : providerName;
    const QJsonObject provider = providers().value(selected).toObject();
    QString type = provider.value(QStringLiteral("type")).toString();
    if (type.isEmpty() && selected == QStringLiteral("herenow"))
        type = QStringLiteral("herenow");
    if (type != QStringLiteral("herenow")) {
        publish(bundleDir, slug, providerName, access);
        return;
    }
    if (provider.isEmpty()) {
        emit failed(QStringLiteral("Publish provider '%1' is not configured.")
                        .arg(selected));
        return;
    }
    if (!validHereApiBase(provider)) {
        emit failed(QStringLiteral(
            "The here.now api_base override must be an HTTP loopback URL."));
        return;
    }
    const QString safeSlug = slugify(slug);
    QString siteSlug = provider.value(QStringLiteral("sites")).toObject()
                           .value(safeSlug).toString();
    if (siteSlug.isEmpty())
        siteSlug = safeSlug;
    const QString apiKey = provider.value(QStringLiteral("api_key")).toString();
    QString claimToken = provider.value(QStringLiteral("claim_tokens")).toObject()
                             .value(siteSlug).toString();
    if (claimToken.isEmpty())
        claimToken = provider.value(QStringLiteral("claim_token")).toString();
    if (apiKey.isEmpty() && claimToken.isEmpty()) {
        emit failed(QStringLiteral(
            "Republishing an anonymous here.now Site needs its claim_token. "
            "Claim the Site or add the token to this provider."));
        return;
    }
    if (accessMode(access) != QStringLiteral("anyone_with_link")
        && apiKey.isEmpty()) {
        emit failed(QStringLiteral(
            "Claim and sign in to the here.now Site before using password or "
            "restricted access."));
        return;
    }
    if (!provider.value(QStringLiteral("domain")).toString().isEmpty()
        && apiKey.isEmpty()) {
        emit failed(QStringLiteral(
            "Claim and sign in to the here.now Site before using a custom domain."));
        return;
    }
    if (access == QStringLiteral("password")
        && (apiKey.isEmpty()
            || provider.value(QStringLiteral("password")).toString().isEmpty())) {
        emit failed(QStringLiteral(
            "Password republish needs a signed-in provider and a configured password."));
        return;
    }
    QString error;
    QSharedPointer<QTemporaryDir> snapshot;
    const QList<BundleFile> files = bundleFiles(bundleDir, &error, &snapshot);
    if (!error.isEmpty()) {
        emit failed(error);
        return;
    }
    if (!d->begin())
        return;
    auto context = QSharedPointer<HerePublishContext>::create();
    context->provider = provider;
    context->providerName = selected;
    if (!claimToken.isEmpty())
        context->provider.insert(QStringLiteral("claim_token"), claimToken);
    context->files = files;
    context->snapshot = snapshot;
    context->requestedSlug = safeSlug;
    context->slug = siteSlug;
    context->access = access;
    context->update = true;
    if (accessMode(access) != QStringLiteral("anyone_with_link")) {
        context->accessBeforeUpdate = true;
        d->applyHereAccess(context);
    } else {
        d->startHere(context);
    }
}

void Publisher::requestVersions(const QString &slug,
                                const QString &providerName) {
    reloadConfig();
    if (!d->requireReadableConfig())
        return;
    const QString selected = providerName.isEmpty() ? defaultProvider() : providerName;
    const QJsonObject provider = providers().value(selected).toObject();
    QString type = provider.value(QStringLiteral("type")).toString();
    if (type.isEmpty() && selected == QStringLiteral("herenow"))
        type = QStringLiteral("herenow");
    if (type != QStringLiteral("herenow")
        || provider.value(QStringLiteral("api_key")).toString().isEmpty()) {
        emit failed(QStringLiteral(
            "Version history needs a signed-in here.now provider."));
        return;
    }
    if (!validHereApiBase(provider)) {
        emit failed(QStringLiteral(
            "The here.now api_base override must be an HTTP loopback URL."));
        return;
    }
    if (!d->begin())
        return;
    const QString requestedSlug = slugify(slug);
    QString siteSlug = provider.value(QStringLiteral("sites")).toObject()
                           .value(requestedSlug).toString();
    if (siteSlug.isEmpty())
        siteSlug = requestedSlug;
    QNetworkReply *reply = d->hereRequest(
        QByteArrayLiteral("GET"),
        QStringLiteral("/api/v1/publish/%1/versions").arg(siteSlug),
        provider, {}, false);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, siteSlug] {
        const QByteArray responseBody = reply->readAll();
        if (!replySucceeded(reply)) {
            const QString message = replyFailure(
                reply, QStringLiteral("Loading here.now version history"),
                responseBody);
            reply->deleteLater();
            d->fail(message);
            return;
        }
        const QJsonArray versions = jsonObject(responseBody)
                                        .value(QStringLiteral("versions")).toArray();
        reply->deleteLater();
        d->setBusy(false);
        emit versionsReceived(siteSlug, versions);
    });
}

void Publisher::revert(const QString &slug, const QString &versionId,
                       const QString &providerName) {
    reloadConfig();
    if (!d->requireReadableConfig())
        return;
    const QString selected = providerName.isEmpty() ? defaultProvider() : providerName;
    const QJsonObject provider = providers().value(selected).toObject();
    QString type = provider.value(QStringLiteral("type")).toString();
    if (type.isEmpty() && selected == QStringLiteral("herenow"))
        type = QStringLiteral("herenow");
    if (type != QStringLiteral("herenow")
        || provider.value(QStringLiteral("api_key")).toString().isEmpty()) {
        emit failed(QStringLiteral("Revert needs a signed-in here.now provider."));
        return;
    }
    if (!validHereApiBase(provider)) {
        emit failed(QStringLiteral(
            "The here.now api_base override must be an HTTP loopback URL."));
        return;
    }
    if (versionId.trimmed().isEmpty()) {
        emit failed(QStringLiteral("Choose a version before reverting."));
        return;
    }
    if (!d->begin())
        return;
    const QString requestedSlug = slugify(slug);
    QString siteSlug = provider.value(QStringLiteral("sites")).toObject()
                           .value(requestedSlug).toString();
    if (siteSlug.isEmpty())
        siteSlug = requestedSlug;
    QNetworkReply *reply = d->hereRequest(
        QByteArrayLiteral("POST"),
        QStringLiteral("/api/v1/publish/%1/versions/%2/restore")
            .arg(siteSlug, versionId), provider, {}, false);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, siteSlug, versionId] {
        const QByteArray responseBody = reply->readAll();
        if (!replySucceeded(reply)) {
            const QString message = replyFailure(
                reply, QStringLiteral("Restoring the here.now version"), responseBody);
            reply->deleteLater();
            d->fail(message);
            return;
        }
        const QJsonObject response = jsonObject(responseBody);
        const QString liveUrl = response.value(QStringLiteral("siteUrl")).toString();
        reply->deleteLater();
        d->setBusy(false);
        emit reverted(liveUrl, siteSlug, versionId);
    });
}

void Publisher::requestSignInCode(const QString &email) {
    const QString cleanEmail = email.trimmed();
    if (!cleanEmail.contains(u'@')) {
        emit failed(QStringLiteral("Enter a valid email address for here.now sign-in."));
        return;
    }
    if (!d->begin())
        return;
    QNetworkReply *reply = d->hereRequest(
        QByteArrayLiteral("POST"), QStringLiteral("/api/auth/agent/request-code"),
        {}, QJsonObject{{QStringLiteral("email"), cleanEmail}});
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] {
        const QByteArray responseBody = reply->readAll();
        if (!replySucceeded(reply)) {
            const QString message = replyFailure(
                reply, QStringLiteral("Requesting the here.now sign-in code"),
                responseBody);
            reply->deleteLater();
            d->fail(message);
            return;
        }
        reply->deleteLater();
        d->setBusy(false);
        emit signInCodeSent();
    });
}

void Publisher::verifySignInCode(const QString &email, const QString &code) {
    const QString cleanEmail = email.trimmed();
    const QString cleanCode = code.trimmed();
    if (!cleanEmail.contains(u'@') || cleanCode.size() < 4) {
        emit failed(QStringLiteral("Enter the email and the complete here.now code."));
        return;
    }
    if (!d->begin())
        return;
    QNetworkReply *reply = d->hereRequest(
        QByteArrayLiteral("POST"), QStringLiteral("/api/auth/agent/verify-code"),
        {}, QJsonObject{{QStringLiteral("email"), cleanEmail},
                        {QStringLiteral("code"), cleanCode},
                        {QStringLiteral("keyName"), QStringLiteral("omapresent")}});
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] {
        const QByteArray responseBody = reply->readAll();
        if (!replySucceeded(reply)) {
            const QString message = replyFailure(
                reply, QStringLiteral("Verifying the here.now sign-in code"),
                responseBody);
            reply->deleteLater();
            d->fail(message);
            return;
        }
        const QString apiKey = jsonObject(responseBody)
                                   .value(QStringLiteral("apiKey")).toString();
        reply->deleteLater();
        if (apiKey.isEmpty()) {
            d->fail(QStringLiteral(
                "here.now accepted the code but did not return an API key."));
            return;
        }
        if (!setProviderKey(QStringLiteral("herenow"),
                            QStringLiteral("api_key"), apiKey)) {
            d->fail(QStringLiteral(
                "Sign-in succeeded, but Omapresent could not save the API key to "
                "%1.").arg(d->configPath));
            return;
        }
        d->setBusy(false);
        emit signedIn();
    });
}

QString Publisher::slugify(const QString &text) {
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString slug;
    slug.reserve(decomposed.size());
    bool separatorPending = false;

    for (const QChar character : decomposed) {
        const QChar::Category category = character.category();
        if (category == QChar::Mark_NonSpacing
            || category == QChar::Mark_SpacingCombining
            || category == QChar::Mark_Enclosing) {
            continue;
        }
        if (character.unicode() < 128 && character.isLetterOrNumber()) {
            if (separatorPending && !slug.isEmpty())
                slug += u'-';
            slug += character.toLower();
            separatorPending = false;
        } else if (character.unicode() < 128 || !character.isLetterOrNumber()) {
            separatorPending = !slug.isEmpty();
        }
    }
    return slug.isEmpty() ? QStringLiteral("deck") : slug;
}

QJsonObject Publisher::parseToml(const QString &tomlText) {
    QJsonObject root;
    QStringList tablePath;
    const QStringList lines = tomlText.split(
        QRegularExpression(QStringLiteral("\\r?\\n")));

    for (const QString &rawLine : lines) {
        const QString line = uncommented(rawLine).trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(u'[') && line.endsWith(u']')
            && !line.startsWith(QStringLiteral("[["))) {
            tablePath = dottedParts(line.mid(1, line.size() - 2));
            continue;
        }

        bool inBasicString = false;
        bool inLiteralString = false;
        bool escaped = false;
        qsizetype equals = -1;
        for (qsizetype i = 0; i < line.size(); ++i) {
            const QChar character = line.at(i);
            if (inBasicString) {
                if (escaped)
                    escaped = false;
                else if (character == u'\\')
                    escaped = true;
                else if (character == u'"')
                    inBasicString = false;
            } else if (inLiteralString) {
                if (character == u'\'')
                    inLiteralString = false;
            } else if (character == u'"') {
                inBasicString = true;
            } else if (character == u'\'') {
                inLiteralString = true;
            } else if (character == u'=') {
                equals = i;
                break;
            }
        }
        if (equals < 1)
            continue;

        QStringList keyPath = tablePath;
        keyPath.append(dottedParts(line.left(equals).trimmed()));
        setNestedValue(root, keyPath, parseTomlValue(line.mid(equals + 1)));
    }
    return root;
}

QString Publisher::patchToml(const QString &tomlText, const QString &dottedKey,
                             const QString &value) {
    const QStringList parts = dottedParts(dottedKey);
    if (parts.isEmpty())
        return tomlText;

    const QString key = parts.constLast();
    const QString table = parts.mid(0, parts.size() - 1).join(u'.');
    const QString newline = tomlText.contains(QStringLiteral("\r\n"))
        ? QStringLiteral("\r\n") : QStringLiteral("\n");
    const QString encodedValue = tomlString(value);
    const QRegularExpression lineExpression(
        QStringLiteral("([^\\r\\n]*)(\\r\\n|\\n|$)"));
    const QRegularExpression tableExpression(
        QStringLiteral("^\\s*\\[\\s*%1\\s*\\]\\s*(?:#.*)?$")
            .arg(QRegularExpression::escape(table)));
    const QRegularExpression keyExpression(
        QStringLiteral("^(\\s*%1\\s*=\\s*)(.*)$")
            .arg(QRegularExpression::escape(key)));

    bool inTargetTable = table.isEmpty();
    bool targetTableFound = table.isEmpty();
    qsizetype targetTableEnd = tomlText.size();
    qsizetype replacementStart = -1;
    qsizetype replacementLength = 0;
    QString replacementLine;
    QRegularExpressionMatchIterator iterator = lineExpression.globalMatch(tomlText);
    while (iterator.hasNext()) {
        const QRegularExpressionMatch lineMatch = iterator.next();
        if (lineMatch.capturedLength(0) == 0)
            break;
        const QString line = lineMatch.captured(1);
        const QString semanticLine = uncommented(line).trimmed();
        if (semanticLine.startsWith(u'[') && semanticLine.endsWith(u']')) {
            if (inTargetTable && targetTableFound)
                targetTableEnd = lineMatch.capturedStart(0);
            inTargetTable = tableExpression.match(line).hasMatch();
            targetTableFound = targetTableFound || inTargetTable;
        }

        if (!inTargetTable)
            continue;
        const QRegularExpressionMatch keyMatch = keyExpression.match(line);
        if (!keyMatch.hasMatch())
            continue;
        const QString replacement = keyMatch.captured(1) + encodedValue
            + valueSuffix(keyMatch.captured(2));
        // parseToml() keeps the last repeated key. Patch that effective value
        // when a malformed file repeats a table or assignment.
        replacementStart = lineMatch.capturedStart(1);
        replacementLength = lineMatch.capturedLength(1);
        replacementLine = replacement;
    }

    if (replacementStart >= 0) {
        QString result = tomlText;
        result.replace(replacementStart, replacementLength, replacementLine);
        return result;
    }

    const QString assignment = key + QStringLiteral(" = ") + encodedValue + newline;
    if (targetTableFound) {
        QString insertion = assignment;
        if (targetTableEnd > 0
            && tomlText.at(targetTableEnd - 1) != u'\n'
            && tomlText.at(targetTableEnd - 1) != u'\r') {
            insertion.prepend(newline);
        }
        QString result = tomlText;
        result.insert(targetTableEnd, insertion);
        return result;
    }

    QString result = tomlText;
    if (!result.isEmpty() && !result.endsWith(u'\n') && !result.endsWith(u'\r'))
        result += newline;
    if (!result.isEmpty() && !result.endsWith(newline + newline))
        result += newline;
    if (!table.isEmpty())
        result += u'[' + table + u']' + newline;
    result += assignment;
    return result;
}
