// STUB — awaiting implementation. See src/omarchytheme.h for the contract and
// tasks/ for the brief. Replace this file wholesale; do not edit the header's
// existing declarations.
#include "omarchytheme.h"

OmarchyTheme::OmarchyTheme(QObject *parent) : QObject(parent) {}
OmarchyTheme::~OmarchyTheme() = default;
QString OmarchyTheme::name() const { return m_name; }
void OmarchyTheme::setOverrideTheme(const QString &t) { m_overrideTheme = t; reload(); }
QString OmarchyTheme::overrideTheme() const { return m_overrideTheme; }
QJsonObject OmarchyTheme::palette() const { return m_palette; }
QString OmarchyTheme::backgroundImagePath() const { return m_backgroundImagePath; }
QStringList OmarchyTheme::installedThemes() { return {}; }
QJsonObject OmarchyTheme::parseColorsToml(const QString &) { return {}; }
double OmarchyTheme::contrastRatio(const QString &, const QString &) { return 1.0; }
QString OmarchyTheme::ensureContrast(const QString &f, const QString &, double) { return f; }
void OmarchyTheme::reload() {}
void OmarchyTheme::watchThemeLink() {}
