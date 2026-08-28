// STUB — awaiting implementation. See src/publisher.h for the contract and
// tasks/ for the brief. Replace this file wholesale; do not edit the header's
// existing declarations.
#include "publisher.h"

Publisher::Publisher(QObject *parent) : QObject(parent) {}
Publisher::~Publisher() = default;
bool Publisher::busy() const { return false; }
QJsonObject Publisher::providers() const { return {}; }
QString Publisher::defaultProvider() const { return QStringLiteral("herenow"); }
void Publisher::reloadConfig() {}
bool Publisher::setProviderKey(const QString &, const QString &, const QString &) { return false; }
void Publisher::publish(const QString &, const QString &, const QString &, const QString &) { emit failed(QStringLiteral("not implemented")); }
void Publisher::requestSignInCode(const QString &) {}
void Publisher::verifySignInCode(const QString &, const QString &) {}
QString Publisher::slugify(const QString &t) { return t; }
QJsonObject Publisher::parseToml(const QString &) { return {}; }
QString Publisher::patchToml(const QString &t, const QString &, const QString &) { return t; }
