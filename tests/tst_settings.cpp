#include <QtTest>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "testrunner.h"
#include "settings.h"

class SettingsTest : public QObject {
    Q_OBJECT

private slots:
    void defaultsWhenNoFilePresent() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString missingPath = tempDir.filePath(QStringLiteral("nonexistent.toml"));

        Settings settings;
        settings.setPath(missingPath);

        // Editor defaults
        QCOMPARE(settings.numberValue(QStringLiteral("editor.text_scale")), 1.0);
        QCOMPARE(settings.stringValue(QStringLiteral("editor.dark_mode")), QStringLiteral("auto"));
        QCOMPARE(settings.stringValue(QStringLiteral("editor.font")), QStringLiteral(""));
        QCOMPARE(settings.stringValue(QStringLiteral("editor.theme")), QStringLiteral(""));
        QCOMPARE(settings.boolValue(QStringLiteral("editor.auto_break_triple_return")), true);
        QCOMPARE(settings.boolValue(QStringLiteral("editor.remember_geometry")), true);

        // Presentation defaults
        QCOMPARE(settings.boolValue(QStringLiteral("presentation.inhibit_idle")), true);
        QCOMPARE(settings.boolValue(QStringLiteral("presentation.do_not_disturb")), true);
        QCOMPARE(settings.stringValue(QStringLiteral("presentation.default_aspect")), QStringLiteral("16:9"));
        QCOMPARE(settings.boolValue(QStringLiteral("presentation.single_monitor_notes")), false);
        // Off by default: saving a deck must not reach the network on its own.
        QCOMPARE(settings.boolValue(QStringLiteral("presentation.auto_prefetch_video")), false);

        // Export defaults
        QCOMPARE(settings.stringValue(QStringLiteral("export.pdf_aspect")), QStringLiteral("16:9"));
        // export.pdf_paginated is gone: spec §8 makes pagination the only legal
        // behaviour, so the key could never have had a second value.
        QVERIFY(!settings.value(QStringLiteral("export.pdf_paginated")).isValid());

        // Unknown key returns invalid QVariant
        QVERIFY(!settings.value(QStringLiteral("editor.unknown_key")).isValid());
        QVERIFY(!settings.value(QStringLiteral("unknown_section.key")).isValid());
    }

    void partialFileOverridesOnlySpecifiedKeys() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("settings.toml"));

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray toml =
            "[editor]\n"
            "text_scale = 1.25\n"
            "dark_mode = \"dark\"\n"
            "\n"
            "[presentation]\n"
            "single_monitor_notes = true\n";
        file.write(toml);
        file.close();

        Settings settings;
        settings.setPath(path);

        // Overridden keys
        QCOMPARE(settings.numberValue(QStringLiteral("editor.text_scale")), 1.25);
        QCOMPARE(settings.stringValue(QStringLiteral("editor.dark_mode")), QStringLiteral("dark"));
        QCOMPARE(settings.boolValue(QStringLiteral("presentation.single_monitor_notes")), true);

        // Unspecified keys retain their default
        QCOMPARE(settings.boolValue(QStringLiteral("editor.auto_break_triple_return")), true);
        QCOMPARE(settings.boolValue(QStringLiteral("presentation.inhibit_idle")), true);
        QCOMPARE(settings.stringValue(QStringLiteral("presentation.default_aspect")), QStringLiteral("16:9"));

        // Check resolved object structure
        const QJsonObject res = settings.resolved();
        QCOMPARE(res.value(QStringLiteral("editor")).toObject().value(QStringLiteral("text_scale")).toDouble(), 1.25);
        QCOMPARE(res.value(QStringLiteral("editor")).toObject().value(QStringLiteral("dark_mode")).toString(), QStringLiteral("dark"));
        QCOMPARE(res.value(QStringLiteral("editor")).toObject().value(QStringLiteral("remember_geometry")).toBool(), true);
    }

    void invalidEnumsFallBackToDefaultsWithWarning() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("settings.toml"));

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray toml =
            "[editor]\n"
            "dark_mode = \"neon\"\n"
            "\n"
            "[presentation]\n"
            "default_aspect = \"99:1\"\n"
            "\n"
            "[export]\n"
            "pdf_aspect = \"invalid\"\n";
        file.write(toml);
        file.close();

        Settings settings;
        settings.setPath(path);

        // Invalid enums fall back to defaults
        QCOMPARE(settings.stringValue(QStringLiteral("editor.dark_mode")), QStringLiteral("auto"));
        QCOMPARE(settings.stringValue(QStringLiteral("presentation.default_aspect")), QStringLiteral("16:9"));
        QCOMPARE(settings.stringValue(QStringLiteral("export.pdf_aspect")), QStringLiteral("16:9"));
    }

    void setValueRoundTripsAndWritesAtomically() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("subdir/settings.toml"));

        Settings settings;
        settings.setPath(path);

        QVERIFY(settings.setValue(QStringLiteral("editor.text_scale"), 1.5));
        QVERIFY(settings.setValue(QStringLiteral("editor.dark_mode"), QStringLiteral("light")));
        QVERIFY(settings.setValue(QStringLiteral("presentation.inhibit_idle"), false));
        QVERIFY(settings.setValue(QStringLiteral("presentation.default_aspect"), QStringLiteral("4:3")));

        // Reading back from same instance
        QCOMPARE(settings.numberValue(QStringLiteral("editor.text_scale")), 1.5);
        QCOMPARE(settings.stringValue(QStringLiteral("editor.dark_mode")), QStringLiteral("light"));
        QCOMPARE(settings.boolValue(QStringLiteral("presentation.inhibit_idle")), false);
        QCOMPARE(settings.stringValue(QStringLiteral("presentation.default_aspect")), QStringLiteral("4:3"));

        // Reading from fresh instance from disk
        Settings fresh;
        fresh.setPath(path);
        QCOMPARE(fresh.numberValue(QStringLiteral("editor.text_scale")), 1.5);
        QCOMPARE(fresh.stringValue(QStringLiteral("editor.dark_mode")), QStringLiteral("light"));
        QCOMPARE(fresh.boolValue(QStringLiteral("presentation.inhibit_idle")), false);
        QCOMPARE(fresh.stringValue(QStringLiteral("presentation.default_aspect")), QStringLiteral("4:3"));

        // Reject invalid keys and invalid enum values
        QVERIFY(!settings.setValue(QStringLiteral("editor.unknown_key"), true));
        QVERIFY(!settings.setValue(QStringLiteral("editor.dark_mode"), QStringLiteral("invalid_mode")));
        QVERIFY(!settings.setValue(QStringLiteral("presentation.default_aspect"), QStringLiteral("21:9")));
    }

    void setValuePreservesCommentsAndLayout() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("settings.toml"));

        const QString initialToml = QStringLiteral(
            "# Custom settings configuration\n"
            "[editor]\n"
            "# Text scaling\n"
            "text_scale = 1.0 # default scale\n"
            "dark_mode = \"auto\"\n"
            "unknown_custom_key = \"keep me verbatim\"\n"
            "\n"
            "# Presentation options\n"
            "[presentation]\n"
            "inhibit_idle = true\n");

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(initialToml.toUtf8());
        file.close();

        Settings settings;
        settings.setPath(path);

        QVERIFY(settings.setValue(QStringLiteral("editor.dark_mode"), QStringLiteral("dark")));

        QFile resultFile(path);
        QVERIFY(resultFile.open(QIODevice::ReadOnly));
        const QString resultToml = QString::fromUtf8(resultFile.readAll());
        resultFile.close();

        // Check comments and unknown keys are intact
        QVERIFY(resultToml.contains(QStringLiteral("# Custom settings configuration")));
        QVERIFY(resultToml.contains(QStringLiteral("# Text scaling")));
        QVERIFY(resultToml.contains(QStringLiteral("text_scale = 1.0 # default scale")));
        QVERIFY(resultToml.contains(QStringLiteral("unknown_custom_key = \"keep me verbatim\"")));
        QVERIFY(resultToml.contains(QStringLiteral("# Presentation options")));
        QVERIFY(resultToml.contains(QStringLiteral("dark_mode = \"dark\"")));
    }

    void defaultsMatchSkillReferenceDocumentation() {
        // Read skill/reference/settings-toml.md and ensure every key in defaults() is documented
        const QString skillDocPath = QStringLiteral("../skill/reference/settings-toml.md");
        QFile file(skillDocPath);
        if (!file.exists()) {
            // Check direct path if running from root
            file.setFileName(QStringLiteral("skill/reference/settings-toml.md"));
        }
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString content = QString::fromUtf8(file.readAll());
        file.close();

        const QJsonObject defaults = Settings::defaults();
        for (auto sectionIt = defaults.constBegin(); sectionIt != defaults.constEnd(); ++sectionIt) {
            const QString section = sectionIt.key();
            const QJsonObject sectionObj = sectionIt.value().toObject();
            for (auto keyIt = sectionObj.constBegin(); keyIt != sectionObj.constEnd(); ++keyIt) {
                const QString key = keyIt.key();
                // Ensure key is present in documentation text
                QVERIFY2(content.contains(key),
                         qPrintable(QStringLiteral("Key '%1.%2' missing from skill/reference/settings-toml.md")
                                    .arg(section, key)));
            }
        }
    }

    void fileWatcherEmitsSettingsChangedOnDiskModification() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("settings.toml"));

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[editor]\ntext_scale = 1.0\n");
        file.close();

        Settings settings;
        settings.setPath(path);

        QSignalSpy spy(&settings, &Settings::settingsChanged);

        // Modify file externally
        QTest::qWait(60);
        QFile modifyFile(path);
        QVERIFY(modifyFile.open(QIODevice::WriteOnly));
        modifyFile.write("[editor]\ntext_scale = 1.75\n");
        modifyFile.close();

        // Wait for debounce timer to fire
        QVERIFY(spy.wait(500));
        QCOMPARE(settings.numberValue(QStringLiteral("editor.text_scale")), 1.75);
    }
};

OMAPRESENT_TEST_SUITE(SettingsTest)
#include "tst_settings.moc"
