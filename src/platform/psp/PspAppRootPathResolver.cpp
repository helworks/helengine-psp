#include "platform/psp/PspAppRootPathResolver.hpp"

#include <stdexcept>
#include <unistd.h>

namespace helengine::psp {
    /// Resolves the PSP homebrew app root path from the launched executable path or process working directory.
    std::string PspAppRootPathResolver::ResolveAppRootPath(const std::string& executablePath) const {
        if (!executablePath.empty()) {
            std::size_t separatorIndex = executablePath.find_last_of("/\\");
            if (separatorIndex != std::string::npos) {
                return executablePath.substr(0, separatorIndex);
            }
        }

        char workingDirectory[512];
        if (getcwd(workingDirectory, sizeof(workingDirectory)) == nullptr) {
            throw std::runtime_error("PSP app root path could not be resolved from the current working directory.");
        }

        return workingDirectory;
    }
}
