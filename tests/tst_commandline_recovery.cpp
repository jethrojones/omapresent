#include <QtTest>

#include "commandlinepolicy.h"
#include "testrunner.h"

class CommandLineRecoveryTest final : public QObject {
    Q_OBJECT

private slots:
    void explicitEditFileTakesPriorityOverRecovery()
    {
        QCOMPARE(CommandLinePolicy::chooseStartupSource(
                     CommandLinePolicy::LaunchMode::Edit, true, true),
                 CommandLinePolicy::StartupSource::ExplicitFile);
    }

    void explicitPresentFileTakesPriorityOverRecovery()
    {
        QCOMPARE(CommandLinePolicy::chooseStartupSource(
                     CommandLinePolicy::LaunchMode::Present, true, true),
                 CommandLinePolicy::StartupSource::ExplicitFile);
    }

    void fileFreeLaunchLeavesRecoveryUntouched()
    {
        QCOMPARE(CommandLinePolicy::chooseStartupSource(
                     CommandLinePolicy::LaunchMode::Edit, false, true),
                 CommandLinePolicy::StartupSource::RecoveredDocument);
    }

    void explicitFileOpenResultControlsFailure()
    {
        QCOMPARE(CommandLinePolicy::explicitFileOpenFailed(
                     CommandLinePolicy::StartupSource::ExplicitFile, true), false);
        QCOMPARE(CommandLinePolicy::explicitFileOpenFailed(
                     CommandLinePolicy::StartupSource::ExplicitFile, false), true);
        QCOMPARE(CommandLinePolicy::explicitFileOpenFailed(
                     CommandLinePolicy::StartupSource::RecoveredDocument, false), false);
    }
};

OMAPRESENT_TEST_SUITE(CommandLineRecoveryTest)
#include "tst_commandline_recovery.moc"
