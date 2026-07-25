#pragma once

#include <string>

#include "IContentStreamSource.hpp"

namespace helengine::psp {
    /// Opens cooked runtime content stored beside the PSP homebrew EBOOT on the memory card.
    class PspMemoryCardContentStreamSource final : public ::IContentStreamSource {
    public:
        /// Creates a source rooted at the directory containing the PSP homebrew application and its staged content.
        explicit PspMemoryCardContentStreamSource(std::string contentRootPath);

        /// Opens one cooked asset using its content-relative runtime path.
        /// <param name="assetPath">Content-relative cooked asset path requested by the generated runtime.</param>
        /// <returns>A readable stream that owns the opened memory-card file.</returns>
        ::Stream* OpenRead(std::string assetPath) override;

    private:
        /// Stores the memory-card directory that contains the staged runtime content.
        std::string ContentRootPath;

        /// Combines the configured content root with one runtime-relative cooked asset path.
        /// <param name="assetPath">Content-relative cooked asset path requested by the generated runtime.</param>
        /// <returns>Physical memory-card path for the requested cooked asset.</returns>
        std::string ResolvePhysicalPath(const std::string& assetPath) const;
    };
}
