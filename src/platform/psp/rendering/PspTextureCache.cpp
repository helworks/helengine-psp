#include "platform/psp/rendering/PspTextureCache.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "platform/psp/rendering/PspRuntimeTexture.hpp"

namespace helengine::psp::rendering {
    namespace {
        /// Packs one raw RGBA texel into the ABGR8888 layout expected by the PSP texture unit.
        std::uint32_t PackRgbaToAbgr(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
            return (static_cast<std::uint32_t>(alpha) << 24)
                | (static_cast<std::uint32_t>(blue) << 16)
                | (static_cast<std::uint32_t>(green) << 8)
                | static_cast<std::uint32_t>(red);
        }
    }

    /// Builds one PSP runtime texture from the cooked texture asset, reusing a cached instance when possible.
    RuntimeTexture* PspTextureCache::BuildTextureFromRaw(TextureAsset* data) {
        if (data == nullptr) {
            throw std::invalid_argument("PSP runtime texture data is required.");
        }

        const std::string cacheKey = data->get_Id();
        if (!cacheKey.empty()) {
            auto cachedTextureIterator = CachedTextures.find(cacheKey);
            if (cachedTextureIterator != CachedTextures.end()) {
                return cachedTextureIterator->second;
            }
        }

        PspRuntimeTexture* runtimeTexture = CreateTexture(data);
        if (!cacheKey.empty()) {
            CachedTextures.emplace(cacheKey, runtimeTexture);
        }

        return runtimeTexture;
    }

    /// Creates one uncached PSP runtime texture from the cooked texture payload.
    PspRuntimeTexture* PspTextureCache::CreateTexture(TextureAsset* data) {
        if (data == nullptr) {
            throw std::invalid_argument("PSP runtime texture data is required.");
        } else if (data->Colors == nullptr) {
            throw std::runtime_error("PSP runtime texture data must include RGBA color bytes.");
        }

        const std::size_t expectedByteCount = static_cast<std::size_t>(data->Width) * static_cast<std::size_t>(data->Height) * 4u;
        if (static_cast<std::size_t>(data->Colors->Length) != expectedByteCount) {
            throw std::runtime_error("PSP runtime texture data length does not match width * height * 4.");
        }

        PspRuntimeTexture* runtimeTexture = new PspRuntimeTexture();
        runtimeTexture->set_Id(data->get_Id());
        runtimeTexture->set_Width(data->Width);
        runtimeTexture->set_Height(data->Height);
        runtimeTexture->SetPixelsAbgr8888(ConvertRgbaBytesToAbgr8888(data));
        return runtimeTexture;
    }

    /// Converts raw RGBA bytes into PSP-ready ABGR8888 texels.
    std::vector<std::uint32_t> PspTextureCache::ConvertRgbaBytesToAbgr8888(TextureAsset* data) {
        const std::size_t pixelCount = static_cast<std::size_t>(data->Width) * static_cast<std::size_t>(data->Height);
        std::vector<std::uint32_t> pixels;
        pixels.reserve(pixelCount);

        for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
            const std::size_t byteOffset = pixelIndex * 4u;
            pixels.push_back(PackRgbaToAbgr(
                static_cast<std::uint8_t>(data->Colors->Data[byteOffset]),
                static_cast<std::uint8_t>(data->Colors->Data[byteOffset + 1u]),
                static_cast<std::uint8_t>(data->Colors->Data[byteOffset + 2u]),
                static_cast<std::uint8_t>(data->Colors->Data[byteOffset + 3u])));
        }

        return pixels;
    }
}
