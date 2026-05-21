#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "RuntimeTexture.hpp"
#include "TextureAsset.hpp"

namespace helengine::psp::rendering {
    class PspRuntimeTexture;

    /// Builds and caches PSP runtime textures from cooked raw texture assets.
    class PspTextureCache final {
    public:
        /// Builds one PSP runtime texture from the cooked texture asset, reusing a cached instance when possible.
        RuntimeTexture* BuildTextureFromRaw(TextureAsset* data);

        /// Releases one PSP runtime texture and removes any matching cache entry.
        void ReleaseTexture(PspRuntimeTexture* texture);

        /// Deletes any PSP runtime textures that were retired earlier in the frame after the renderer reaches a safe boundary.
        void FlushReleasedTextures();

    private:
        /// Creates one uncached PSP runtime texture from the cooked texture payload.
        PspRuntimeTexture* CreateTexture(TextureAsset* data);

        /// Computes the expected cooked color-payload byte length for one texture asset.
        static std::size_t GetExpectedColorByteCount(TextureAsset* data);

        /// Converts one cooked texture payload into PSP-ready ABGR8888 texels.
        static std::vector<std::uint32_t> ConvertTextureToAbgr8888(TextureAsset* data);

        /// Converts one quantized channel back to 8-bit precision using the supplied bit count.
        static std::uint8_t ExpandChannelTo8Bit(std::uint8_t value, int bitCount);

        /// Reads one palette-backed texel and packs it into PSP-ready ABGR8888 layout.
        static std::uint32_t ReadIndexedPalettePixel(TextureAsset* data, std::size_t paletteIndex);

        /// Stores cached PSP runtime textures by deterministic runtime asset id.
        std::unordered_map<std::uint64_t, PspRuntimeTexture*> CachedTextures;

        /// Stores detached PSP pixel buffers that must stay alive until the renderer reaches a safe frame boundary.
        std::vector<std::vector<std::uint32_t>> ReleasedTexturePixelBuffers;
    };
}
