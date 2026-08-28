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
};

OMAPRESENT_TEST_SUITE(CommandLineRecoveryTest)
#include "tst_commandline_recovery.moc"
