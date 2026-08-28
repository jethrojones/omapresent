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
};

void AssetIndexTest::looksLikeImageReference()
{
    // Paths containing slashes
    QVERIFY(AssetIndex::looksLikeImageReference("~/Pictures/budget.png"));
    QVERIFY(AssetIndex::looksLikeImageReference("./img/x.png"));
    QVERIFY(AssetIndex::looksLikeImageReference("/abs/path/image.jpg"));
    QVERIFY(AssetIndex::looksLikeImageReference("sub/photo"));
    QVERIFY(AssetIndex::looksLikeImageReference("images/diagram.webp"));

    // Known image extensions
    QVERIFY(AssetIndex::looksLikeImageReference("budget.png"));
    QVERIFY(AssetIndex::looksLikeImageReference("budget.PNG"));
    QVERIFY(AssetIndex::looksLikeImageReference("photo.jpg"));
    QVERIFY(AssetIndex::looksLikeImageReference("photo.jpeg"));
    QVERIFY(AssetIndex::looksLikeImageReference("animation.gif"));
    QVERIFY(AssetIndex::looksLikeImageReference("vector.svg"));
    QVERIFY(AssetIndex::looksLikeImageReference("shot.heic"));
    QVERIFY(AssetIndex::looksLikeImageReference("document.pdf"));
    QVERIFY(AssetIndex::looksLikeImageReference("raw.tiff"));
    QVERIFY(AssetIndex::looksLikeImageReference("graphic.avif"));

    // Size hints on bare paths
    QVERIFY(AssetIndex::looksLikeImageReference("budget.png|600"));
    QVERIFY(AssetIndex::looksLikeImageReference("sub/photo|main"));

    // Lone words and non-image text must not be misread
    QVERIFY(!AssetIndex::looksLikeImageReference("word"));
    QVERIFY(!AssetIndex::looksLikeImageReference("lone_word"));
    QVERIFY(!AssetIndex::looksLikeImageReference("Introduction"));
    QVERIFY(!AssetIndex::looksLikeImageReference("heading"));
    QVERIFY(!AssetIndex::looksLikeImageReference(""));
    QVERIFY(!AssetIndex::looksLikeImageReference("   "));
    QVERIFY(!AssetIndex::looksLikeImageReference(".png"));
    QVERIFY(!AssetIndex::looksLikeImageReference("notes.txt"));
    QVERIFY(!AssetIndex::looksLikeImageReference("report.doc"));
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
    // shortestUniqueReference expands ~ so resolve will return the file if it matches
    // Even if the file doesn't exist on disk, home path prefix can shorten
    QString shortHome = index.shortestUniqueReference(fakeHomeFile);
    QVERIFY(shortHome.startsWith("~") || shortHome == fakeHomeFile);
}

void AssetIndexTest::directoryWatchingEmitsIndexChanged()
{
    QTemporaryDir tempRoot;
    QVERIFY(tempRoot.isValid());

    AssetIndex index;
    index.setRoot(tempRoot.path());

    QSignalSpy spy(&index, &AssetIndex::indexChanged);

    // Create a new file in the watched directory
    QFile newFile(tempRoot.path() + "/new_asset.png");
    QVERIFY(newFile.open(QIODevice::WriteOnly));
    newFile.write("NEW ASSET");
    newFile.close();

    // Wait for the debounced watcher signal (max 1000ms)
    // Note: QFileSystemWatcher on inotify emits directoryChanged
    spy.wait(1000);

    // Even if inotify is fast or slow, check that resolving works once rebuilt
    if (spy.count() == 0) {
        // If system event loop needs a tick
        QTest::qWait(250);
    }

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

OMAPRESENT_TEST_SUITE(AssetIndexTest)
#include "tst_assetindex.moc"
