#include "renderhost.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLibraryInfo>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineProfile>

namespace {

const QString rendererUrl = QStringLiteral("qrc:/renderer/render.html");

// Everything the page still has to fetch before a PDF is worth printing.
const QString readyProbe = QStringLiteral(
    "(!document.fonts || document.fonts.status === 'loaded')"
    " && Array.prototype.every.call(document.images, function (i) { return i.complete; })");

// The renderer may need a moment for fonts, images and KaTeX; give it five
// seconds at 100ms a look, then print whatever is on the page.
constexpr int settleIntervalMs = 100;
constexpr int maxSettleAttempts = 50;

QString webChannelClient() {
    // Some Qt builds compile the client into a resource; a distribution build
    // ships it on disk beside the other Qt data files.
    const QStringList candidates{
        QStringLiteral(":/qtwebchannel/qwebchannel.js"),
        QLibraryInfo::path(QLibraryInfo::DataPath)
            + QStringLiteral("/webchannel/qwebchannel.js")};

    for (const QString &candidate : candidates) {
        QFile file(candidate);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString::fromUtf8(file.readAll());
    }
    return {};
}

}  // namespace

RenderHost::RenderHost(QObject *parent) : QObject(parent) {}

void RenderHost::state(const QString &json) {
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isObject())
        return;

    m_state = document.object();
    m_slideIndex = m_state.value(QStringLiteral("slideIndex")).toInt();
    m_slideCount = m_state.value(QStringLiteral("slideCount")).toInt();
    m_scrollFraction = m_state.value(QStringLiteral("scrollFraction")).toDouble();
    emit stateChanged();
}

QJsonObject RenderHost::composeDeck(const QString &mode, const QJsonObject &deck,
                                    const QJsonObject &assets, const QJsonObject &media,
                                    const QJsonObject &palette,
                                    const QString &backgroundImagePath, qreal textScale) {
    QString backgroundImage = backgroundImagePath;
    if (!backgroundImage.isEmpty() && !backgroundImage.contains(QStringLiteral("://")))
        backgroundImage = QUrl::fromLocalFile(backgroundImage).toString();

    return QJsonObject{
        {QStringLiteral("mode"), mode},
        {QStringLiteral("frontmatter"), deck.value(QStringLiteral("frontmatter")).toObject()},
        {QStringLiteral("slides"), deck.value(QStringLiteral("slides")).toArray()},
        {QStringLiteral("assets"), assets},
        {QStringLiteral("media"), media},
        {QStringLiteral("palette"), palette},
        {QStringLiteral("backgroundImage"), backgroundImage},
        {QStringLiteral("textScale"), textScale}};
}

QString RenderHost::callScript(const QString &function, const QJsonObject &deckJson) {
    QString json = QString::fromUtf8(QJsonDocument(deckJson).toJson(QJsonDocument::Compact));
    // JSON strings may hold U+2028 and U+2029 raw; a JavaScript source literal
    // may not, and this JSON is about to become one.
    json.replace(QChar(0x2028), QStringLiteral("\\u2028"));
    json.replace(QChar(0x2029), QStringLiteral("\\u2029"));

    return QStringLiteral("window.omapresent && window.omapresent.%1(%2);")
        .arg(function, json);
}

QPageLayout RenderHost::pageLayoutFor(const QString &aspect) {
    double wide = 16;
    double tall = 9;
    const QStringList parts = aspect.split(QLatin1Char(':'));
    if (parts.size() == 2) {
        bool wideOk = false;
        bool tallOk = false;
        const double parsedWide = parts.at(0).trimmed().toDouble(&wideOk);
        const double parsedTall = parts.at(1).trimmed().toDouble(&tallOk);
        if (wideOk && tallOk && parsedWide > 0 && parsedTall > 0) {
            wide = parsedWide;
            tall = parsedTall;
        }
    }

    // 960pt is 13.33in, the long edge of the usual 16:9 slide canvas. QPageSize
    // wants the page in portrait and QPageLayout turns it, so build the short
    // edge from the ratio and let the orientation say which way round it goes.
    constexpr double longEdge = 960.0;
    const double shortEdge = longEdge * qMin(wide, tall) / qMax(wide, tall);
    const QPageSize size(QSizeF(shortEdge, longEdge), QPageSize::Point,
                         QString(), QPageSize::ExactMatch);
    return QPageLayout(size,
                       wide >= tall ? QPageLayout::Landscape : QPageLayout::Portrait,
                       QMarginsF());
}

QString RenderHost::bridgeScript() {
    const QString client = webChannelClient();
    if (client.isEmpty())
        return {};

    return QStringLiteral(R"JS((function () {
%1
    if (!window.qt || !qt.webChannelTransport)
        return;
    new QWebChannel(qt.webChannelTransport, function (channel) {
        window.omapresentHost = channel.objects.omapresentHost;
        if (window.omapresent) {
            window.omapresent.onState = function (state) {
                window.omapresentHost.state(JSON.stringify(state));
            };
        }
    });
})();
)JS").arg(client);
}

PdfExport::PdfExport(QObject *parent) : QObject(parent) {
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(settleIntervalMs);
    connect(&m_settleTimer, &QTimer::timeout, this, &PdfExport::printWhenSettled);
}

PdfExport::~PdfExport() = default;

void PdfExport::run(const QJsonObject &deckJson, const QString &outputPath) {
    if (m_busy) {
        emit finished(false, outputPath, QStringLiteral("An export is already running."));
        return;
    }

    m_busy = true;
    m_deck = deckJson;
    m_outputPath = outputPath;
    m_settleAttempts = 0;

    if (!m_page) {
        // Off-the-record: an export must not leave cookies or a cache behind.
        m_profile = new QWebEngineProfile(this);
        m_page = new QWebEnginePage(m_profile, this);
        connect(m_page, &QWebEnginePage::pdfPrintingFinished, this,
                [this](const QString &path, bool success) {
                    m_busy = false;
                    emit finished(success, path,
                                  success ? QString()
                                          : QStringLiteral("The PDF could not be written."));
                });
        connect(m_page, &QWebEnginePage::loadFinished, this, [this](bool ok) {
            if (!ok) {
                fail(QStringLiteral("The renderer could not be loaded."));
                return;
            }
            renderDeck();
        });
        m_page->load(QUrl(rendererUrl));
        return;
    }

    renderDeck();
}

void PdfExport::renderDeck() {
    m_settleAttempts = 0;
    m_page->runJavaScript(RenderHost::callScript(QStringLiteral("render"), m_deck),
                          [this](const QVariant &) { printWhenSettled(); });
}

void PdfExport::printWhenSettled() {
    m_page->runJavaScript(readyProbe, [this](const QVariant &ready) {
        if (!ready.toBool() && ++m_settleAttempts < maxSettleAttempts) {
            m_settleTimer.start();
            return;
        }

        const QString aspect = m_deck.value(QStringLiteral("frontmatter"))
                                   .toObject()
                                   .value(QStringLiteral("aspect"))
                                   .toString();
        m_page->printToPdf(m_outputPath, RenderHost::pageLayoutFor(aspect));
    });
}

void PdfExport::fail(const QString &message) {
    m_busy = false;
    emit finished(false, m_outputPath, message);
}
