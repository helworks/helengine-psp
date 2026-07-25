#include "platform/psp/PspMemoryCardContentStreamSource.hpp"

#include <stdexcept>
#include <utility>

#include "system/io/file-stream.hpp"

namespace helengine::psp {
    /// Creates a source rooted at the directory containing the PSP homebrew application and its staged content.
    PspMemoryCardContentStreamSource::PspMemoryCardContentStreamSource(std::string contentRootPath)
        : ContentRootPath(std::move(contentRootPath)) {
        if (ContentRootPath.empty()) {
            throw std::invalid_argument("PSP memory-card content root path is required.");
        }
    }

    /// Opens one cooked asset using its content-relative runtime path.
    ::Stream* PspMemoryCardContentStreamSource::OpenRead(std::string assetPath) {
        return new FileStream(ResolvePhysicalPath(assetPath), FileMode::Open, FileAccess::Read, FileShare::Read);
    }

    /// Combines the configured content root with one runtime-relative cooked asset path.
    std::string PspMemoryCardContentStreamSource::ResolvePhysicalPath(const std::string& assetPath) const {
        if (assetPath.empty()) {
            throw std::invalid_argument("PSP cooked asset path is required.");
        }

        if (ContentRootPath.back() == '/' || ContentRootPath.back() == '\\') {
            return ContentRootPath + assetPath;
        }

        return ContentRootPath + "/" + assetPath;
    }
}
