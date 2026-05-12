#pragma once

#include <string>

namespace helengine::psp {
    /// Writes simple boot diagnostics into the PSP app working directory so runtime path failures can be inspected after launch.
    class PspBootTrace {
    public:
        /// Sets the resolved PSP app root used for trace-file writes.
        static void SetAppRootPath(const std::string& appRootPath);

        /// Appends one diagnostic line to the PSP boot trace file.
        static void WriteLine(const std::string& message);
    };
}
