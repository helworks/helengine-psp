#pragma once

#include <string>
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

    private:
        /// Creates one uncached PSP runtime texture from the cooked texture payload.
        PspRuntimeTexture* CreateTexture(TextureAsset* data);

        /// Converts raw RGBA bytes into PSP-ready ABGR8888 texels.
        static std::vector<std::uint32_t> ConvertRgbaBytesToAbgr8888(TextureAsset* data);

        /// Stores cached PSP runtime textures by cooked texture asset id.
        std::unordered_map<std::string, PspRuntimeTexture*> CachedTextures;
    };
}
