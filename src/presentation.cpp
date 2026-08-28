// STUB — awaiting implementation. See src/presentation.h for the contract and
// tasks/ for the brief. Replace this file wholesale; do not edit the header's
// existing declarations.
#include "presentation.h"

Presentation::Presentation(QObject *parent) : QObject(parent) {}
Presentation::~Presentation() = default;
bool Presentation::active() const { return false; }
int Presentation::slideIndex() const { return 0; }
int Presentation::slideCount() const { return 0; }
int Presentation::elapsedSeconds() const { return 0; }
void Presentation::start(int) {}
void Presentation::stop() {}
void Presentation::resetTimer() {}
void Presentation::setDeck(const QJsonObject &) {}
void Presentation::next() {}
void Presentation::previous() {}
void Presentation::gotoSlide(int) {}
void Presentation::scrollBy(qreal) {}
void Presentation::showRecall(const QString &) {}
void Presentation::hideRecall() {}
void Presentation::setBlank(const QString &) {}
void Presentation::setOverview(bool) {}
void Presentation::toggleNotesOverlay() {}
QStringList Presentation::outputs() const { return {}; }
void Presentation::assignMonitors() {}
void Presentation::inhibitIdle(bool) {}
void Presentation::setDoNotDisturb(bool) {}
