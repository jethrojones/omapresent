// STUB — awaiting implementation. See src/assetindex.h for the contract and
// tasks/ for the brief. Replace this file wholesale; do not edit the header's
// existing declarations.
#include "assetindex.h"

AssetIndex::AssetIndex(QObject *parent) : QObject(parent) {}
AssetIndex::~AssetIndex() = default;
void AssetIndex::setDeckDir(const QString &d) { m_deckDir = d; }
QString AssetIndex::deckDir() const { return m_deckDir; }
void AssetIndex::setRoot(const QString &r) { m_root = r; }
QString AssetIndex::root() const { return m_root.isEmpty() ? m_deckDir : m_root; }
QString AssetIndex::resolve(const QString &) const { return {}; }
QJsonObject AssetIndex::resolveAll(const QStringList &) const { return {}; }
QStringList AssetIndex::extractReferences(const QString &) { return {}; }
bool AssetIndex::looksLikeImageReference(const QString &) { return false; }
void AssetIndex::parseSizeHint(const QString &r, QString *b, int *w, bool *m) { if (b) *b = r; if (w) *w = 0; if (m) *m = false; }
QString AssetIndex::shortestUniqueReference(const QString &p) const { return p; }
void AssetIndex::rebuild() {}
