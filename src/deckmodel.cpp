// STUB — awaiting implementation. See src/deckmodel.h for the contract and
// tasks/ for the brief. Replace this file wholesale; do not edit the header's
// existing declarations.
#include "deckmodel.h"

DeckModel::DeckModel(QObject *parent) : QObject(parent) {}
void DeckModel::setSource(const QString &text) { m_source = text; emit deckChanged(); }
QString DeckModel::source() const { return m_source; }
QString DeckModel::frontmatterRaw() const { return m_frontmatterRaw; }
QVariantMap DeckModel::frontmatter() const { return m_frontmatter; }
QVector<Slide> DeckModel::slides() const { return m_slides; }
int DeckModel::slideCount() const { return m_slides.size(); }
int DeckModel::slideIndexForLine(int) const { return -1; }
QJsonObject DeckModel::toJson() const { return {}; }
QString DeckModel::stripComments(const QString &text) { return text; }
bool DeckModel::isSeparatorLine(const QString &, const QString &, const QString &) { return false; }
void DeckModel::parseSeparatorTag(const QString &, QString *k, bool *s) { if (k) k->clear(); if (s) *s = false; }
QVariantMap DeckModel::parseFrontmatter(const QString &) { return {}; }
