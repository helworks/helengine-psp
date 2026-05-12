#pragma once

#include <string>

namespace helengine::psp {
    /// Resolves the running PSP homebrew app directory that owns EBOOT.PBP and cooked assets.
    class PspAppRootPathResolver {
    public:
        /// Resolves the PSP homebrew app root path from the launched executable path or process working directory.
        std::string ResolveAppRootPath(const std::string& executablePath) const;
    };
}
