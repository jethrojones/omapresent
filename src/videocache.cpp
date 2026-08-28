// STUB — awaiting implementation. See src/videocache.h for the contract and
// tasks/ for the brief. Replace this file wholesale; do not edit the header's
// existing declarations.
#include "videocache.h"

VideoCache::VideoCache(QObject *parent) : QObject(parent) {}
VideoCache::~VideoCache() = default;
void VideoCache::setDeckDir(const QString &d) { m_deckDir = d; }
QString VideoCache::cacheDir() const { return m_deckDir + QStringLiteral("/.omapresent-cache"); }
QJsonObject VideoCache::describe(const QString &) const { return {}; }
void VideoCache::prefetch(const QStringList &) { emit prefetchFinished({}); }
VideoCache::Host VideoCache::hostFor(const QString &) { return NotAVideo; }
bool VideoCache::isBareUrlLine(const QString &) { return false; }
QStringList VideoCache::extractUrls(const QString &) { return {}; }
QString VideoCache::embedUrlFor(const QString &) { return {}; }
