#include <QtTest>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QSignalSpy>
#include <QFile>
#include <QDir>
#include <QUrl>

#include "testrunner.h"
#include "assetindex.h"

class AssetIndexTest : public QObject {
    Q_OBJECT

private slots:
    void looksLikeImageReference();
    void parseSizeHint();
    void extractReferencesFromSlide();
    void extractReferencesIgnoresCodeBlocksAndLoneWords();
    void extractReferencesIgnoresInlineCodeAndQr();
    void resolveRelativePathDeckDir();
    void resolveTildeAndEnvExpansion();
    void resolveShortestWinsAtDifferentDepths();
    void resolveCaseInsensitiveRetry();
    void resolveSpacesInPaths();
    void resolveBrokenSymlink();
    void resolveMissingReturnsEmpty();
    void resolveHttpUrlsUnchanged();
    void resolveAllBuildsJsonObject();
    void shortestUniqueReference();
    void directoryWatchingEmitsIndexChanged();
    void rootNonExistentDoesNotCrash();
    void deckDirFallbackWhenRootEmpty();
    void subfolderRelativePathMatching();
    void multipleInlineImagesInOrder();
    void resolveMaintainsOldIndexDuringRebuild();
    void directoryWatchingCapsAtLimit();
};

void AssetIndexTest::looksLikeImageReference()
{
    // Whole lines that are an image path. Spec §4.5: a path with spaces needs
    // no escaping, so the extension has to carry those on its own.
    const QStringList images = {
        QStringLiteral("~/Pictures/budget.png"),
        QStringLiteral("./img/chart with spaces.png"),
        QStringLiteral("./img/x.png"),
        QStringLiteral("/abs/path/photo.jpeg"),
        QStringLiteral("/abs/path/image.jpg"),
        QStringLiteral("images/diagram.webp"),
        QStringLiteral("budget.png"),
        QStringLiteral("logo.svg"),
        QStringLiteral("budget.PNG"),
        QStringLiteral("photo.jpg"),
        QStringLiteral("animation.gif"),
        QStringLiteral("shot.heic"),
        QStringLiteral("document.pdf"),
        QStringLiteral("raw.tiff"),
        QStringLiteral("graphic.avif"),
        // Extensionless, but rooted like a path and naming something.
        QStringLiteral("~/photos/holiday"),
        QStringLiteral("./img/diagram"),
        QStringLiteral("../shared/logo"),
        QStringLiteral("/mnt/photos/scan"),
        // Size hints come off before the decision.
        QStringLiteral("budget.png|600"),
        QStringLiteral("./img/photo|main"),
    };
    for (const QString &line : images)
        QVERIFY2(AssetIndex::looksLikeImageReference(line), qPrintable(line));

    // Prose. Per spec §4.2 every one of these is a speaker note, and a note
    // read as an image paints the missing-image placeholder over the slide
    // (spec §4.5 step 5). Containing a slash is not enough to be a path.
    const QStringList prose = {
        QStringLiteral("and/or"),
        QStringLiteral("X/Twitter"),
        QStringLiteral("budget"),
        QStringLiteral("word"),
        QStringLiteral("lone_word"),
        QStringLiteral("Introduction"),
        QStringLiteral("heading"),
        QStringLiteral("The ratio is 16:9 and the file lives in ~/Documents/aibrain somewhere"),
        QStringLiteral("Recognised hosts: YouTube, Vimeo, X/Twitter, Instagram"),
        QStringLiteral("Recognised hosts: YouTube, Vimeo, Loom, Descript, TikTok, X/Twitter"),
        QStringLiteral("$$e^{i\\pi} + 1 = 0$$"),
        QStringLiteral("Omapresent supports line comments with `//`"),
        QStringLiteral("Omapresent supports line comments with `//`, "
                       "Obsidian comments with %%...%%"),
        QStringLiteral("Reads and/or writes, then returns"),
        QStringLiteral(""),
        QStringLiteral("   "),
        QStringLiteral(".png"),
        QStringLiteral("notes.txt"),
        QStringLiteral("report.doc"),
        // Roots that name nothing.
        QStringLiteral("/"),
        QStringLiteral("//"),
        QStringLiteral("~/"),
        QStringLiteral("./"),
        // A relative path with neither a root nor an extension is
        // indistinguishable from "and/or", so it went with it. `sub/photo.png`
        // still works, and so does `./sub/photo`.
        QStringLiteral("sub/photo"),
    };
    for (const QString &line : prose)
        QVERIFY2(!AssetIndex::looksLikeImageReference(line), qPrintable(line));
}

void AssetIndexTest::extractReferencesIgnoresInlineCodeAndQr()
{
    // Syntax shown to the reader inside backticks is documentation about the
    // syntax, not a use of it. The renderer draws none of these, and a
    // reference we invent here resolves to nothing and paints the
    // missing-image placeholder over the slide (spec §4.5 step 5).
    QCOMPARE(AssetIndex::extractReferences(
                 QStringLiteral("A table cell holding `![[figure.png]]` as example syntax")),
             QStringList());
    QCOMPARE(AssetIndex::extractReferences(QStringLiteral(
                 "- Obsidian embeds: `![[diagram.png]]` or `![[diagram.png|600]]`")),
             QStringList());
    QCOMPARE(AssetIndex::extractReferences(
                 QStringLiteral("Write `![alt](photo.png)` for a Markdown image")),
             QStringList());
    // Double backticks so the span itself can contain one.
    QCOMPARE(AssetIndex::extractReferences(
                 QStringLiteral("Escaped: ``![[a.png]] and `` after")),
             QStringList());

    // An unclosed backtick is not a code span, so the rest of the line is
    // still read normally.
    QCOMPARE(AssetIndex::extractReferences(
                 QStringLiteral("Unclosed ` then ![[real.png]]")),
             QStringList({QStringLiteral("real.png")}));

    // Outside backticks the same embeds are real references again.
    QCOMPARE(AssetIndex::extractReferences(QStringLiteral("![[figure.png]]")),
             QStringList({QStringLiteral("figure.png")}));
    QCOMPARE(AssetIndex::extractReferences(
                 QStringLiteral("![[one.png]] and ![[two.png]]")),
             QStringList({QStringLiteral("one.png"), QStringLiteral("two.png")}));

    // `qr:` forces a QR code (spec §4.8); it never names a file, and the
    // renderer's parseObsidianImage excludes it, so this side must agree.
    QCOMPARE(AssetIndex::extractReferences(QStringLiteral("![[qr:https://example.com]]")),
             QStringList());
    QCOMPARE(AssetIndex::extractReferences(QStringLiteral("![[QR:https://example.com]]")),
             QStringList());
    QCOMPARE(AssetIndex::extractReferences(QStringLiteral("![](qr:https://example.com)")),
             QStringList());
    // A file that merely starts with the letters is not a QR.
    QCOMPARE(AssetIndex::extractReferences(QStringLiteral("![[qrcode.png]]")),
             QStringList({QStringLiteral("qrcode.png")}));
}

void AssetIndexTest::parseSizeHint()
{
    QString bare;
    int width = -1;
    bool isMain = true;

    // Plain reference
    AssetIndex::parseSizeHint("photo.png", &bare, &width, &isMain);
    QCOMPARE(bare, QString("photo.png"));
    QCOMPARE(width, 0);
    QCOMPARE(isMain, false);

    // Max width in px
    AssetIndex::parseSizeHint("photo.png|600", &bare, &width, &isMain);
    QCOMPARE(bare, QString("photo.png"));
    QCOMPARE(width, 600);
    QCOMPARE(isMain, false);

    // Max width with px suffix
    AssetIndex::parseSizeHint("path/to/photo.png|1200px", &bare, &width, &isMain);
    QCOMPARE(bare, QString("path/to/photo.png"));
    QCOMPARE(width, 1200);
    QCOMPARE(isMain, false);

    // Bento hero |main and |hero
    AssetIndex::parseSizeHint("hero.png|main", &bare, &width, &isMain);
    QCOMPARE(bare, QString("hero.png"));
    QCOMPARE(width, 0);
    QCOMPARE(isMain, true);

    AssetIndex::parseSizeHint("hero.png|MAIN", &bare, &width, &isMain);
    QCOMPARE(bare, QString("hero.png"));
    QCOMPARE(width, 0);
    QCOMPARE(isMain, true);

    AssetIndex::parseSizeHint("hero.png|hero", &bare, &width, &isMain);
    QCOMPARE(bare, QString("hero.png"));
    QCOMPARE(width, 0);
    QCOMPARE(isMain, true);

    // Nullptr safety
    AssetIndex::parseSizeHint("safe.png|400", nullptr, nullptr, nullptr);
}

void AssetIndexTest::extractReferencesFromSlide()
{
    // A slide containing all four reference forms:
    // 1. ![[budget.png]] — Obsidian embed
    // 2. ![alt](budget.png) — standard Markdown
    // 3. ![[~/Pictures/budget.png]], ![[/abs/path/x.png]], ![[../img/x.png]]
    // 4. bare path alone on a line: ~/Pictures/budget.png or ./img/x.png or image.png
    const QString slide = QStringLiteral(
        "# Quarterly Review\n"
        "\n"
        "![[budget.png|600]]\n"
        "![Chart](assets/chart.png)\n"
        "![[~/Pictures/vacation.jpg|main]]\n"
        "![[/abs/path/overview.png]]\n"
        "![[../img/details.svg]]\n"
        "\n"
        "./img/bare_relative.png\n"
        "~/Pictures/bare_home.png\n"
        "bare_extension.png\n"
        "\n"
        "A regular paragraph here.\n"
    );

    QStringList refs = AssetIndex::extractReferences(slide);
    QStringList expected = {
        "budget.png",
        "assets/chart.png",
        "~/Pictures/vacation.jpg",
        "/abs/path/overview.png",
        "../img/details.svg",
        "./img/bare_relative.png",
        "~/Pictures/bare_home.png",
        "bare_extension.png"
    };

    QCOMPARE(refs, expected);
}

void AssetIndexTest::extractReferencesIgnoresCodeBlocksAndLoneWords()
{
    const QString slide = QStringLiteral(
        "# Slide Title\n"
        "\n"
        "lone_word\n"
        "Introduction\n"
        "Heading\n"
        "\n"
        "![[real_image.png]]\n"
        "\n"
        "```markdown\n"
        "![[code_block_embed.png]]\n"
        "![alt](code_block_md.png)\n"
        "~/Pictures/code_bare.png\n"
        "```\n"
        "\n"
        "~~~cpp\n"
        "// Some code\n"
        "![[tildes_embed.png]]\n"
        "~~~\n"
        "\n"
        "another_lone_word\n"
        "bare_final.jpg\n"
    );

    QStringList refs = AssetIndex::extractReferences(slide);
    QStringList expected = {
        "real_image.png",
        "bare_final.jpg"
    };

    QCOMPARE(refs, expected);
}

void AssetIndexTest::resolveRelativePathDeckDir()
{
    QTemporaryDir tempDeck;
    QVERIFY(tempDeck.isValid());

    // Create deckDir/img/budget.png
    QDir(tempDeck.path()).mkpath("img");
    QFile file(tempDeck.path() + "/img/budget.png");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("PNG DATA");
    file.close();

    AssetIndex index;
    index.setDeckDir(tempDeck.path());
    index.waitForIndex();

    // Exact relative path
    QString resolved = index.resolve("img/budget.png");
    QCOMPARE(resolved, QDir::cleanPath(tempDeck.path() + "/img/budget.png"));

    // Relative with ./
    QString resolvedDot = index.resolve("./img/budget.png");
    QCOMPARE(resolvedDot, QDir::cleanPath(tempDeck.path() + "/img/budget.png"));
}

void AssetIndexTest::resolveTildeAndEnvExpansion()
{
    QTemporaryDir tempHome;
    QVERIFY(tempHome.isValid());

    // Create a temporary file in tempHome
    QFile file(tempHome.path() + "/env_test.png");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("DATA");
    file.close();

    qputenv("OMAPRESENT_TEST_ASSET_DIR", tempHome.path().toLocal8Bit());

    AssetIndex index;
    QString resolved = index.resolve("$OMAPRESENT_TEST_ASSET_DIR/env_test.png");
    QCOMPARE(resolved, QDir::cleanPath(tempHome.path() + "/env_test.png"));

    QString resolvedBraces = index.resolve("${OMAPRESENT_TEST_ASSET_DIR}/env_test.png");
    QCOMPARE(resolvedBraces, QDir::cleanPath(tempHome.path() + "/env_test.png"));

    qunsetenv("OMAPRESENT_TEST_ASSET_DIR");
}

void AssetIndexTest::resolveShortestWinsAtDifferentDepths()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    // Create two files with the same name:
    // 1. /root/sub1/deep/photo.png (depth 2)
    // 2. /root/photo.png (depth 0, shortest)
    QDir(tempRoot.path()).mkpath("sub1/deep");

    QFile deepFile(tempRoot.path() + "/sub1/deep/photo.png");
    QVERIFY(deepFile.open(QIODevice::WriteOnly));
    deepFile.write("DEEP");
    deepFile.close();

    QFile shallowFile(tempRoot.path() + "/photo.png");
    QVERIFY(shallowFile.open(QIODevice::WriteOnly));
    shallowFile.write("SHALLOW");
    shallowFile.close();

    AssetIndex index;
    index.setRoot(tempRoot.path());
    index.waitForIndex();

    // Searching "photo.png" should resolve to the shallowest/shortest match
    QString resolved = index.resolve("photo.png");
    QCOMPARE(resolved, QDir::cleanPath(tempRoot.path() + "/photo.png"));
}

void AssetIndexTest::resolveCaseInsensitiveRetry()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    // Create file with uppercase on disk: Budget_Report.PNG
    QDir(tempRoot.path()).mkpath("assets");
    QFile file(tempRoot.path() + "/assets/Budget_Report.PNG");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("CASE TEST");
    file.close();

    AssetIndex index;
    index.setRoot(tempRoot.path());
    index.waitForIndex();

    // Resolve with all-lowercase reference
    QString resolved = index.resolve("budget_report.png");
    QCOMPARE(resolved, QDir::cleanPath(tempRoot.path() + "/assets/Budget_Report.PNG"));

    // Resolve with mixed case
    QString resolvedMixed = index.resolve("BUDGET_REPORT.png");
    QCOMPARE(resolvedMixed, QDir::cleanPath(tempRoot.path() + "/assets/Budget_Report.PNG"));
}

void AssetIndexTest::resolveSpacesInPaths()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    // Create directory and file with spaces
    QDir(tempRoot.path()).mkpath("Presentation Assets/Q3 Reports");
    QFile file(tempRoot.path() + "/Presentation Assets/Q3 Reports/My Chart 2026.png");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("SPACES DATA");
    file.close();

    AssetIndex index;
    index.setRoot(tempRoot.path());
    index.waitForIndex();

    // Search by filename with spaces
    QString resolved = index.resolve("My Chart 2026.png");
    QCOMPARE(resolved, QDir::cleanPath(tempRoot.path() + "/Presentation Assets/Q3 Reports/My Chart 2026.png"));
}

void AssetIndexTest::resolveBrokenSymlink()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    // Create a broken symlink pointing to nonexistent file
    QString linkPath = tempRoot.path() + "/broken_link.png";
    QFile::link(tempRoot.path() + "/does_not_exist.png", linkPath);

    AssetIndex index;
    index.setRoot(tempRoot.path());
    index.waitForIndex();

    // Broken symlink must not resolve and must not crash
    QString resolved = index.resolve("broken_link.png");
    QVERIFY(resolved.isEmpty());
}

void AssetIndexTest::resolveMissingReturnsEmpty()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AssetIndex index;
    index.setRoot(tempRoot.path());
    index.waitForIndex();

    // Nonexistent reference returns empty string (for Step 5 placeholder)
    QVERIFY(index.resolve("nonexistent_image.png").isEmpty());
    QVERIFY(index.resolve("").isEmpty());
    QVERIFY(index.resolve("   ").isEmpty());
}

void AssetIndexTest::resolveHttpUrlsUnchanged()
{
    AssetIndex index;

    QString httpUrl = "http://example.com/images/hero.png";
    QString httpsUrl = "https://cdn.example.org/assets/slide.jpg?v=2";
    QString dataUrl = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=";

    QCOMPARE(index.resolve(httpUrl), httpUrl);
    QCOMPARE(index.resolve(httpsUrl), httpsUrl);
    QCOMPARE(index.resolve(dataUrl), dataUrl);
}

void AssetIndexTest::resolveAllBuildsJsonObject()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    QFile file(tempRoot.path() + "/budget.png");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("BUDGET");
    file.close();

    AssetIndex index;
    index.setRoot(tempRoot.path());
    index.waitForIndex();

    QStringList refs = {
        "budget.png",
        "missing.png",
        "https://example.com/pic.jpg"
    };

    QJsonObject obj = index.resolveAll(refs);

    QString expectedBudgetUrl = QUrl::fromLocalFile(QDir::cleanPath(tempRoot.path() + "/budget.png")).toString();
    QCOMPARE(obj.value("budget.png").toString(), expectedBudgetUrl);
    QCOMPARE(obj.value("missing.png").toString(), QString(""));
    QCOMPARE(obj.value("https://example.com/pic.jpg").toString(), QString("https://example.com/pic.jpg"));
}

void AssetIndexTest::shortestUniqueReference()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    // Tree structure:
    // /root/unique.png
    // /root/sub_a/duplicate.png
    // /root/sub_b/duplicate.png
    QDir(tempRoot.path()).mkpath("sub_a");
    QDir(tempRoot.path()).mkpath("sub_b");

    QFile uniqueFile(tempRoot.path() + "/unique.png");
    QVERIFY(uniqueFile.open(QIODevice::WriteOnly));
    uniqueFile.write("UNIQUE");
    uniqueFile.close();

    QFile dupA(tempRoot.path() + "/sub_a/duplicate.png");
    QVERIFY(dupA.open(QIODevice::WriteOnly));
    dupA.write("DUP_A");
    dupA.close();

    QFile dupB(tempRoot.path() + "/sub_b/duplicate.png");
    QVERIFY(dupB.open(QIODevice::WriteOnly));
    dupB.write("DUP_B");
    dupB.close();

    AssetIndex index;
    index.setRoot(tempRoot.path());
    index.waitForIndex();

    // 1. Unique file -> returns filename alone
    QString shortUnique = index.shortestUniqueReference(tempRoot.path() + "/unique.png");
    QCOMPARE(shortUnique, QString("unique.png"));

    // 2. Ambiguous file -> returns parent prefix
    QString shortDupA = index.shortestUniqueReference(tempRoot.path() + "/sub_a/duplicate.png");
    QCOMPARE(shortDupA, QString("sub_a/duplicate.png"));

    QString shortDupB = index.shortestUniqueReference(tempRoot.path() + "/sub_b/duplicate.png");
    QCOMPARE(shortDupB, QString("sub_b/duplicate.png"));

    // 3. File in home directory outside root -> ~/...
    QString homePath = QDir::homePath();
    QString fakeHomeFile = homePath + "/test_image_12345.png";
    QString shortHome = index.shortestUniqueReference(fakeHomeFile);
    QVERIFY(shortHome.startsWith("~") || shortHome == fakeHomeFile);
}

void AssetIndexTest::directoryWatchingEmitsIndexChanged()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AssetIndex index;
    index.setRoot(tempRoot.path());
    index.waitForIndex();

    QSignalSpy spy(&index, &AssetIndex::indexChanged);

    // Create a new file in the watched directory
    QFile newFile(tempRoot.path() + "/new_asset.png");
    QVERIFY(newFile.open(QIODevice::WriteOnly));
    newFile.write("NEW ASSET");
    newFile.close();

    // Wait for the debounced watcher signal (max 2000ms)
    spy.wait(2000);
    index.waitForIndex();

    // Now resolving the new asset should succeed
    QString resolved = index.resolve("new_asset.png");
    QCOMPARE(resolved, QDir::cleanPath(tempRoot.path() + "/new_asset.png"));
}

void AssetIndexTest::rootNonExistentDoesNotCrash()
{
    AssetIndex index;
    index.setRoot("/path/that/absolutely/does/not/exist/on/any/system");
    QCOMPARE(index.root(), QString("/path/that/absolutely/does/not/exist/on/any/system"));
    QVERIFY(index.resolve("test.png").isEmpty());
}

void AssetIndexTest::deckDirFallbackWhenRootEmpty()
{
    QTemporaryDir tempDeck;
    QVERIFY(tempDeck.isValid());

    QFile file(tempDeck.path() + "/sample.png");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("SAMPLE");
    file.close();

    AssetIndex index;
    index.setDeckDir(tempDeck.path());
    index.waitForIndex();

    // root() should default to deckDir() when m_root is empty
    QCOMPARE(index.root(), tempDeck.path());

    QString resolved = index.resolve("sample.png");
    QCOMPARE(resolved, QDir::cleanPath(tempDeck.path() + "/sample.png"));
}

void AssetIndexTest::subfolderRelativePathMatching()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    QDir(tempRoot.path()).mkpath("sub1/deep");
    QDir(tempRoot.path()).mkpath("sub2/deep");

    QFile file1(tempRoot.path() + "/sub1/deep/diagram.svg");
    QVERIFY(file1.open(QIODevice::WriteOnly));
    file1.write("SVG1");
    file1.close();

    QFile file2(tempRoot.path() + "/sub2/deep/diagram.svg");
    QVERIFY(file2.open(QIODevice::WriteOnly));
    file2.write("SVG2");
    file2.close();

    AssetIndex index;
    index.setRoot(tempRoot.path());
    index.waitForIndex();

    // Disambiguate by subpath
    QString resolved1 = index.resolve("sub1/deep/diagram.svg");
    QCOMPARE(resolved1, QDir::cleanPath(tempRoot.path() + "/sub1/deep/diagram.svg"));

    QString resolved2 = index.resolve("sub2/deep/diagram.svg");
    QCOMPARE(resolved2, QDir::cleanPath(tempRoot.path() + "/sub2/deep/diagram.svg"));
}

void AssetIndexTest::multipleInlineImagesInOrder()
{
    const QString line = "Text with ![[first.png|300]] and ![second](second.jpg) followed by ![[third.svg|main]]";
    QStringList refs = AssetIndex::extractReferences(line);
    QStringList expected = { "first.png", "second.jpg", "third.svg" };
    QCOMPARE(refs, expected);
}

void AssetIndexTest::resolveMaintainsOldIndexDuringRebuild()
{
    QTemporaryDir tempRoot1;
    QVERIFY(tempRoot1.isValid());
    QFile file1(tempRoot1.path() + "/old_file.png");
    QVERIFY(file1.open(QIODevice::WriteOnly));
    file1.write("OLD");
    file1.close();

    AssetIndex index;
    index.setRoot(tempRoot1.path());
    index.waitForIndex();

    // Initial resolution works
    QCOMPARE(index.resolve("old_file.png"), QDir::cleanPath(tempRoot1.path() + "/old_file.png"));

    QTemporaryDir tempRoot2;
    QVERIFY(tempRoot2.isValid());
    QFile file2(tempRoot2.path() + "/new_file.png");
    QVERIFY(file2.open(QIODevice::WriteOnly));
    file2.write("NEW");
    file2.close();

    // Trigger rebuild by setting new root
    index.setRoot(tempRoot2.path());

    // Immediately before the new scan lands, resolve() must still answer from the previous index
    // rather than returning empty mid-rebuild
    QString midRebuildResolved = index.resolve("old_file.png");
    QCOMPARE(midRebuildResolved, QDir::cleanPath(tempRoot1.path() + "/old_file.png"));

    // Now wait for the new scan to finish
    index.waitForIndex();

    // After new scan lands, old file is gone and new file is resolved
    QVERIFY(index.resolve("old_file.png").isEmpty());
    QCOMPARE(index.resolve("new_file.png"), QDir::cleanPath(tempRoot2.path() + "/new_file.png"));
}

void AssetIndexTest::directoryWatchingCapsAtLimit()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    // Create a hierarchy of folders
    for (int i = 0; i < 20; ++i) {
        QDir(tempRoot.path()).mkpath(QString("dir_%1/sub").arg(i));
        QFile file(QString("%1/dir_%2/sub/pic_%2.png").arg(tempRoot.path()).arg(i));
        if (file.open(QIODevice::WriteOnly)) {
            file.write("DATA");
            file.close();
        }
    }

    AssetIndex index;
    index.setRoot(tempRoot.path());
    index.waitForIndex();

    // Check that assets resolve correctly
    QCOMPARE(index.resolve("pic_0.png"), QDir::cleanPath(QString("%1/dir_0/sub/pic_0.png").arg(tempRoot.path())));
    QCOMPARE(index.resolve("pic_19.png"), QDir::cleanPath(QString("%1/dir_19/sub/pic_19.png").arg(tempRoot.path())));
}

OMAPRESENT_TEST_SUITE(AssetIndexTest)
#include "tst_assetindex.moc"
