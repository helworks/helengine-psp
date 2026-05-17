#include "platform/psp/PspBootTrace.hpp"

#include <pspiofilemgr.h>

namespace helengine::psp {
#if defined(HELENGINE_PSP_ENABLE_BOOT_TRACE) && HELENGINE_PSP_ENABLE_BOOT_TRACE
    namespace {
        /// Relative boot-trace filename written beneath the resolved PSP app root.
        constexpr const char* TraceFileName = "helengine_psp_boot.log";

        /// Debug fallback trace path used when the resolved PSP app root is not available yet.
        constexpr const char* FallbackTraceFilePath = "ms0:/PSP/GAME/HELENGINE/helengine_psp_boot.log";

        /// Resolved PSP app root used for trace-file writes after startup.
        std::string AppRootPath;
    }

    /// Sets the resolved PSP app root used for trace-file writes.
    void PspBootTrace::SetAppRootPath(const std::string& appRootPath) {
        AppRootPath = appRootPath;
    }

    /// Appends one diagnostic line to the PSP boot trace file.
    void PspBootTrace::WriteLine(const std::string& message) {
        std::string traceFilePath = FallbackTraceFilePath;
        if (!AppRootPath.empty()) {
            traceFilePath = AppRootPath + "/" + TraceFileName;
        }

        SceUID fileHandle = sceIoOpen(
            traceFilePath.c_str(),
            PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND,
            0777);
        if (fileHandle < 0) {
            return;
        }

        sceIoWrite(fileHandle, message.c_str(), message.size());
        sceIoWrite(fileHandle, "\n", 1);
        sceIoClose(fileHandle);
    }
#endif
}
