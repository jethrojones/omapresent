#include "settings.h"
#include "publisher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>

namespace {

QString defaultSettingsPath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("omapresent/settings.toml"));
}

bool isValidDarkMode(const QString &value) {
    return value == QStringLiteral("auto")
        || value == QStringLiteral("dark")
        || value == QStringLiteral("light");
}

bool isValidAspect(const QString &value) {
    return value == QStringLiteral("16:9")
        || value == QStringLiteral("4:3")
        || value == QStringLiteral("16:10")
        || value == QStringLiteral("1:1");
}

} // namespace

struct Settings::Private {
    QFileSystemWatcher watcher;
    QTimer reloadTimer;
    mutable QSet<QString> warnedKeys;
};

Settings::Settings(QObject *parent)
    : QObject(parent), d(new Private) {
    d->reloadTimer.setSingleShot(true);
    d->reloadTimer.setInterval(50);
    connect(&d->reloadTimer, &QTimer::timeout, this, &Settings::reload);

    connect(&d->watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
        d->reloadTimer.start();
    });
    connect(&d->watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        d->reloadTimer.start();
    });

    setPath(defaultSettingsPath());
}

Settings::~Settings() {
    delete d;
}

void Settings::setPath(const QString &path) {
    m_path = path.isEmpty() ? defaultSettingsPath() : path;

    const QStringList watchedFiles = d->watcher.files();
    if (!watchedFiles.isEmpty())
        d->watcher.removePaths(watchedFiles);
    const QStringList watchedDirs = d->watcher.directories();
    if (!watchedDirs.isEmpty())
        d->watcher.removePaths(watchedDirs);

    if (QFile::exists(m_path))
        d->watcher.addPath(m_path);
    const QString dir = QFileInfo(m_path).absolutePath();
    if (QDir(dir).exists())
        d->watcher.addPath(dir);

    reload();
}

QString Settings::path() const {
    return m_path;
}

void Settings::reload() {
    d->warnedKeys.clear();

    if (QFile::exists(m_path)) {
        if (!d->watcher.files().contains(m_path))
            d->watcher.addPath(m_path);

        QFile file(m_path);
        if (file.open(QIODevice::ReadOnly)) {
            const QString content = QString::fromUtf8(file.readAll());
            m_fileValues = Publisher::parseToml(content);
        } else {
            m_fileValues = {};
        }
    } else {
        m_fileValues = {};
    }

    const QString dir = QFileInfo(m_path).absolutePath();
    if (QDir(dir).exists() && !d->watcher.directories().contains(dir))
        d->watcher.addPath(dir);

    emit settingsChanged();
}

QJsonObject Settings::defaults() {
    return QJsonObject{
        {QStringLiteral("editor"), QJsonObject{
            {QStringLiteral("text_scale"), 1.0},
            {QStringLiteral("dark_mode"), QStringLiteral("auto")},
            {QStringLiteral("font"), QStringLiteral("")},
            {QStringLiteral("theme"), QStringLiteral("")},
            {QStringLiteral("auto_break_triple_return"), true},
            {QStringLiteral("remember_geometry"), true}
        }},
        {QStringLiteral("presentation"), QJsonObject{
            {QStringLiteral("inhibit_idle"), true},
            {QStringLiteral("do_not_disturb"), true},
            {QStringLiteral("default_aspect"), QStringLiteral("16:9")},
            {QStringLiteral("single_monitor_notes"), false},
            // Off by default: a save must not reach the network without being
            // asked. Spec §4.8 still allows it, and turning this on restores
            // the fetch-on-save behaviour; "Prepare for offline" is the
            // explicit route either way.
            {QStringLiteral("auto_prefetch_video"), false}
        }},
        // `export.pdf_paginated` used to live here. Spec §8 says a slide taller
        // than a page paginates and is never scaled, so the only other setting
        // this key could have had is one the spec forbids. A preference with no
        // legal second value is not a preference.
        {QStringLiteral("export"), QJsonObject{
            {QStringLiteral("pdf_aspect"), QStringLiteral("16:9")}
        }}
    };
}

QVariant Settings::value(const QString &dottedKey) const {
    const QStringList parts = dottedKey.split(u'.', Qt::SkipEmptyParts);
    if (parts.size() != 2)
        return {};

    const QString section = parts.at(0);
    const QString key = parts.at(1);

    const QJsonObject allDefaults = defaults();
    if (!allDefaults.contains(section))
        return {};
    const QJsonObject sectionDefaults = allDefaults.value(section).toObject();
    if (!sectionDefaults.contains(key))
        return {};

    const QJsonValue defaultValue = sectionDefaults.value(key);

    const QJsonObject fileSection = m_fileValues.value(section).toObject();
    if (!fileSection.contains(key))
        return defaultValue.toVariant();

    const QJsonValue fileValue = fileSection.value(key);
    if (fileValue.isUndefined() || fileValue.isNull())
        return defaultValue.toVariant();

    // Type checking and enum validation
    if (defaultValue.isBool()) {
        if (fileValue.isBool())
            return fileValue.toBool();
        if (fileValue.isString()) {
            const QString str = fileValue.toString().trimmed().toLower();
            if (str == QStringLiteral("true"))
                return true;
            if (str == QStringLiteral("false"))
                return false;
        }
        if (!d->warnedKeys.contains(dottedKey)) {
            d->warnedKeys.insert(dottedKey);
            qWarning("Settings: invalid boolean value for %s, falling back to default", qPrintable(dottedKey));
        }
        return defaultValue.toVariant();
    }

    if (defaultValue.isDouble()) {
        if (fileValue.isDouble())
            return fileValue.toDouble();
        if (fileValue.isString()) {
            bool ok = false;
            const double num = fileValue.toString().toDouble(&ok);
            if (ok)
                return num;
        }
        if (!d->warnedKeys.contains(dottedKey)) {
            d->warnedKeys.insert(dottedKey);
            qWarning("Settings: invalid numeric value for %s, falling back to default", qPrintable(dottedKey));
        }
        return defaultValue.toVariant();
    }

    if (defaultValue.isString()) {
        const QString str = fileValue.toString();
        if (dottedKey == QStringLiteral("editor.dark_mode")) {
            if (isValidDarkMode(str))
                return str;
            if (!d->warnedKeys.contains(dottedKey)) {
                d->warnedKeys.insert(dottedKey);
                qWarning("Settings: invalid value '%s' for editor.dark_mode, falling back to 'auto'", qPrintable(str));
            }
            return defaultValue.toVariant();
        }
        if (dottedKey == QStringLiteral("presentation.default_aspect") || dottedKey == QStringLiteral("export.pdf_aspect")) {
            if (isValidAspect(str))
                return str;
            if (!d->warnedKeys.contains(dottedKey)) {
                d->warnedKeys.insert(dottedKey);
                qWarning("Settings: invalid value '%s' for %s, falling back to '16:9'", qPrintable(str), qPrintable(dottedKey));
            }
            return defaultValue.toVariant();
        }
        return str;
    }

    return defaultValue.toVariant();
}

bool Settings::boolValue(const QString &dottedKey) const {
    return value(dottedKey).toBool();
}

QString Settings::stringValue(const QString &dottedKey) const {
    return value(dottedKey).toString();
}

double Settings::numberValue(const QString &dottedKey) const {
    return value(dottedKey).toDouble();
}

bool Settings::setValue(const QString &dottedKey, const QVariant &val) {
    const QStringList parts = dottedKey.split(u'.', Qt::SkipEmptyParts);
    if (parts.size() != 2)
        return false;

    const QString section = parts.at(0);
    const QString key = parts.at(1);

    const QJsonObject allDefaults = defaults();
    if (!allDefaults.contains(section) || !allDefaults.value(section).toObject().contains(key))
        return false;

    // Validate enums before writing
    if (dottedKey == QStringLiteral("editor.dark_mode") && !isValidDarkMode(val.toString()))
        return false;
    if ((dottedKey == QStringLiteral("presentation.default_aspect") || dottedKey == QStringLiteral("export.pdf_aspect"))
        && !isValidAspect(val.toString()))
        return false;

    QString original;
    QFile input(m_path);
    if (input.exists()) {
        if (!input.open(QIODevice::ReadOnly))
            return false;
        original = QString::fromUtf8(input.readAll());
        input.close();
    }

    const QFileInfo fileInfo(m_path);
    if (!QDir().mkpath(fileInfo.absolutePath()))
        return false;

    QString valStr;
    if (val.userType() == QMetaType::Bool) {
        valStr = val.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    } else if (val.userType() == QMetaType::Double || val.userType() == QMetaType::Float) {
        valStr = QString::number(val.toDouble());
    } else if (val.userType() == QMetaType::Int || val.userType() == QMetaType::LongLong) {
        valStr = QString::number(val.toLongLong());
    } else {
        valStr = val.toString();
    }

    const QString patched = Publisher::patchToml(original, dottedKey, valStr);

    QSaveFile output(m_path);
    if (!output.open(QIODevice::WriteOnly))
        return false;
    if (output.write(patched.toUtf8()) < 0)
        return false;
    if (!output.commit())
        return false;

    reload();
    return true;
}

QJsonObject Settings::resolved() const {
    QJsonObject result;
    const QJsonObject allDefaults = defaults();
    for (auto it = allDefaults.constBegin(); it != allDefaults.constEnd(); ++it) {
        const QString section = it.key();
        const QJsonObject sectionDefaults = it.value().toObject();
        QJsonObject resolvedSection;
        for (auto keyIt = sectionDefaults.constBegin(); keyIt != sectionDefaults.constEnd(); ++keyIt) {
            const QString key = keyIt.key();
            const QString dottedKey = section + u'.' + key;
            resolvedSection.insert(key, QJsonValue::fromVariant(value(dottedKey)));
        }
        result.insert(section, resolvedSection);
    }
    return result;
}
