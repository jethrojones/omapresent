#include "commandlinepolicy.h"

namespace CommandLinePolicy {

StartupSource chooseStartupSource(LaunchMode mode, bool hasExplicitFile,
                                  bool recoveryModified)
{
    const bool isWindowLaunch = mode == LaunchMode::Edit
        || mode == LaunchMode::Present;
    if (isWindowLaunch && hasExplicitFile)
        return StartupSource::ExplicitFile;
    return recoveryModified ? StartupSource::RecoveredDocument
                            : StartupSource::ExistingDocument;
}

} // namespace CommandLinePolicy
