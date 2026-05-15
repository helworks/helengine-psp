#include "platform/psp/rendering/PspTextureCache.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include <pspkernel.h>

#include "platform/psp/PspBootTrace.hpp"
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

        const std::uint64_t cacheKey = data->get_RuntimeAssetId();
        PspBootTrace::WriteLine(
            std::string("PspTextureBuildBegin runtimeId=") + std::to_string(cacheKey)
            + " id=" + data->get_Id()
            + " width=" + std::to_string(data->Width)
            + " height=" + std::to_string(data->Height)
            + " colors=" + std::to_string(data->Colors != nullptr ? data->Colors->Length : 0)
            + " freeMem=" + std::to_string(sceKernelTotalFreeMemSize()));
        if (cacheKey != 0u) {
            auto cachedTextureIterator = CachedTextures.find(cacheKey);
            if (cachedTextureIterator != CachedTextures.end()) {
                PspBootTrace::WriteLine(
                    std::string("PspTextureBuildCacheHit runtimeId=") + std::to_string(cacheKey)
                    + " ptr=" + std::to_string(reinterpret_cast<std::uintptr_t>(cachedTextureIterator->second))
                    + " freeMem=" + std::to_string(sceKernelTotalFreeMemSize()));
                return cachedTextureIterator->second;
            }
        }

        PspRuntimeTexture* runtimeTexture = CreateTexture(data);
        if (cacheKey != 0u) {
            CachedTextures.emplace(cacheKey, runtimeTexture);
        }
        PspBootTrace::WriteLine(
            std::string("PspTextureBuildEnd runtimeId=") + std::to_string(cacheKey)
            + " ptr=" + std::to_string(reinterpret_cast<std::uintptr_t>(runtimeTexture))
            + " freeMem=" + std::to_string(sceKernelTotalFreeMemSize()));

        return runtimeTexture;
    }

    /// Releases one PSP runtime texture and removes any matching cache entry.
    void PspTextureCache::ReleaseTexture(PspRuntimeTexture* texture) {
        if (texture == nullptr) {
            throw std::invalid_argument("PSP runtime texture release requires one texture instance.");
        }

        PspBootTrace::WriteLine(
            std::string("PspTextureReleaseQueue ptr=")
            + std::to_string(reinterpret_cast<std::uintptr_t>(texture))
            + " id=" + texture->get_Id()
            + " runtimeId=" + std::to_string(texture->GetRuntimeAssetId())
            + " freeMem=" + std::to_string(sceKernelTotalFreeMemSize()));
        const std::uint64_t cacheKey = texture->GetRuntimeAssetId();
        if (cacheKey != 0u) {
            auto cachedTextureIterator = CachedTextures.find(cacheKey);
            if (cachedTextureIterator != CachedTextures.end() && cachedTextureIterator->second == texture) {
                CachedTextures.erase(cachedTextureIterator);
            }
        }

        ReleasedTextures.push_back(texture);
    }

    /// Deletes any PSP runtime textures that were retired earlier in the frame after the renderer reaches a safe boundary.
    void PspTextureCache::FlushReleasedTextures() {
        if (ReleasedTextures.empty()) {
            return;
        }

        PspBootTrace::WriteLine(
            std::string("PspTextureFlushBegin count=")
            + std::to_string(ReleasedTextures.size())
            + " freeMem=" + std::to_string(sceKernelTotalFreeMemSize()));
        for (PspRuntimeTexture* texture : ReleasedTextures) {
            PspBootTrace::WriteLine(
                std::string("PspTextureFlushDelete ptr=")
                + std::to_string(reinterpret_cast<std::uintptr_t>(texture))
                + " id=" + (texture != nullptr ? texture->get_Id() : std::string())
                + " runtimeId=" + std::to_string(texture != nullptr ? texture->GetRuntimeAssetId() : 0u)
                + " freeMem=" + std::to_string(sceKernelTotalFreeMemSize()));
            delete texture;
        }

        ReleasedTextures.clear();
        PspBootTrace::WriteLine(
            std::string("PspTextureFlushEnd freeMem=")
            + std::to_string(sceKernelTotalFreeMemSize()));
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

        PspBootTrace::WriteLine(
            std::string("PspTextureCreateBegin runtimeId=") + std::to_string(data->get_RuntimeAssetId())
            + " id=" + data->get_Id()
            + " expectedBytes=" + std::to_string(expectedByteCount)
            + " freeMem=" + std::to_string(sceKernelTotalFreeMemSize()));
        PspRuntimeTexture* runtimeTexture = new PspRuntimeTexture();
        PspBootTrace::WriteLine(
            std::string("PspTextureCreateAllocated ptr=")
            + std::to_string(reinterpret_cast<std::uintptr_t>(runtimeTexture))
            + " freeMem=" + std::to_string(sceKernelTotalFreeMemSize()));
        runtimeTexture->set_Id(data->get_Id());
        runtimeTexture->SetRuntimeAssetId(data->get_RuntimeAssetId());
        runtimeTexture->set_Width(data->Width);
        runtimeTexture->set_Height(data->Height);
        runtimeTexture->SetPixelsAbgr8888(ConvertRgbaBytesToAbgr8888(data));
        PspBootTrace::WriteLine(
            std::string("PspTextureCreateReady ptr=")
            + std::to_string(reinterpret_cast<std::uintptr_t>(runtimeTexture))
            + " pixelCount=" + std::to_string(runtimeTexture->GetPixelCount())
            + " freeMem=" + std::to_string(sceKernelTotalFreeMemSize()));
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
