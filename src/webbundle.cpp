// STUB — awaiting implementation. See src/webbundle.h for the contract and
// tasks/T11-web-bundle.md for the brief.
#include "webbundle.h"

WebBundle::WebBundle(QObject *parent) : QObject(parent) {}
WebBundle::~WebBundle() = default;
void WebBundle::setDeck(const QJsonObject &deck) { m_deck = deck; }
void WebBundle::setDeckDir(const QString &dir) { m_deckDir = dir; }
bool WebBundle::build(const QString &) { m_lastError = QStringLiteral("not implemented"); return false; }
QStringList WebBundle::files() const { return m_files; }
qint64 WebBundle::totalBytes() const { return m_totalBytes; }
QString WebBundle::lastError() const { return m_lastError; }
