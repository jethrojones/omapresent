#pragma once

namespace CommandLinePolicy {

enum class LaunchMode {
    Edit,
    Present,
};

enum class StartupSource {
    ExistingDocument,
    RecoveredDocument,
    ExplicitFile,
};

StartupSource chooseStartupSource(LaunchMode mode, bool hasExplicitFile,
                                  bool recoveryModified);

bool explicitFileOpenFailed(StartupSource source, bool openSucceeded);

} // namespace CommandLinePolicy
