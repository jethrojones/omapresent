// STUB — awaiting implementation. See src/settings.h for the contract and
// tasks/T10-settings.md for the brief.
#include "settings.h"

Settings::Settings(QObject *parent) : QObject(parent) {}
Settings::~Settings() = default;
void Settings::setPath(const QString &path) { m_path = path; }
QString Settings::path() const { return m_path; }
void Settings::reload() {}
QVariant Settings::value(const QString &) const { return {}; }
bool Settings::boolValue(const QString &) const { return false; }
QString Settings::stringValue(const QString &) const { return {}; }
double Settings::numberValue(const QString &) const { return 0.0; }
bool Settings::setValue(const QString &, const QVariant &) { return false; }
QJsonObject Settings::resolved() const { return {}; }
QJsonObject Settings::defaults() { return {}; }
