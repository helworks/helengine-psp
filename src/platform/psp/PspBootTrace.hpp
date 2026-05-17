#pragma once

#include <string>

namespace helengine::psp {
    /// Writes simple boot diagnostics into the PSP app working directory so runtime path failures can be inspected after launch.
    class PspBootTrace {
    public:
        /// Sets the resolved PSP app root used for trace-file writes.
#if defined(HELENGINE_PSP_ENABLE_BOOT_TRACE) && HELENGINE_PSP_ENABLE_BOOT_TRACE
        static void SetAppRootPath(const std::string& appRootPath);
#else
        static void SetAppRootPath(const std::string& appRootPath) {
            (void)appRootPath;
        }
#endif

        /// Appends one diagnostic line to the PSP boot trace file.
#if defined(HELENGINE_PSP_ENABLE_BOOT_TRACE) && HELENGINE_PSP_ENABLE_BOOT_TRACE
        static void WriteLine(const std::string& message);
#else
        static void WriteLine(const std::string& message) {
            (void)message;
        }
#endif
    };
}
