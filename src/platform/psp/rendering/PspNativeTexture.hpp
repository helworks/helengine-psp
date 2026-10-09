#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "platform/psp/rendering/PspNativeTextureLayout.hpp"

namespace helengine::psp::rendering {
    /// Validates PGT1 data and provides CPU sampling for the existing static font surface cache.
    class PspNativeTexture final {
    public:
        /// Validates complete canonical extents, storage and every palette index before returning metadata.
        static PspNativeTextureLayout Read(const std::uint8_t* colors, std::size_t colorBytes, std::size_t paletteBytes, std::uint32_t width, std::uint32_t height);
        /// Decodes validated GPU storage into a padded ABGR8888 CPU surface only when CPU text sampling requests it.
        static std::vector<std::uint32_t> Decode(const std::uint8_t* colors, const std::uint8_t* palette, const PspNativeTextureLayout& layout);
    };
}
