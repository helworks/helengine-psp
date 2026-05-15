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

        /// Converts raw RGBA bytes into PSP-ready ABGR8888 texels.
        static std::vector<std::uint32_t> ConvertRgbaBytesToAbgr8888(TextureAsset* data);

        /// Stores cached PSP runtime textures by deterministic runtime asset id.
        std::unordered_map<std::uint64_t, PspRuntimeTexture*> CachedTextures;

        /// Stores PSP runtime textures that have been removed from cache but must not be deleted until the renderer reaches a safe frame boundary.
        std::vector<PspRuntimeTexture*> ReleasedTextures;
    };
}
