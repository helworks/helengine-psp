#include "platform/psp/rendering/PspTextureCache.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include <pspkernel.h>

#include "TextureAssetColorFormat.hpp"
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
            + " palette=" + std::to_string(data->PaletteColors != nullptr ? data->PaletteColors->Length : 0)
            + " format=" + std::to_string(static_cast<int>(data->ColorFormat))
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

        std::vector<std::uint32_t> releasedPixels = texture->TakePixelsAbgr8888();
        if (!releasedPixels.empty()) {
            ReleasedTexturePixelBuffers.push_back(std::move(releasedPixels));
        }
    }

    /// Deletes any PSP runtime textures that were retired earlier in the frame after the renderer reaches a safe boundary.
    void PspTextureCache::FlushReleasedTextures() {
        if (ReleasedTexturePixelBuffers.empty()) {
            return;
        }

        PspBootTrace::WriteLine(
            std::string("PspTextureFlushBegin count=")
            + std::to_string(ReleasedTexturePixelBuffers.size())
            + " freeMem=" + std::to_string(sceKernelTotalFreeMemSize()));

        ReleasedTexturePixelBuffers.clear();
        PspBootTrace::WriteLine(
            std::string("PspTextureFlushEnd freeMem=")
            + std::to_string(sceKernelTotalFreeMemSize()));
    }

    /// Creates one uncached PSP runtime texture from the cooked texture payload.
    PspRuntimeTexture* PspTextureCache::CreateTexture(TextureAsset* data) {
        if (data == nullptr) {
            throw std::invalid_argument("PSP runtime texture data is required.");
        } else if (data->Colors == nullptr || data->Colors->Length == 0) {
            throw std::runtime_error("PSP runtime texture data must include cooked color payload bytes.");
        }

        const std::size_t expectedByteCount = GetExpectedColorByteCount(data);
        if (static_cast<std::size_t>(data->Colors->Length) != expectedByteCount) {
            throw std::runtime_error("PSP runtime texture data length does not match the cooked texture format.");
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
        runtimeTexture->SetPixelsAbgr8888(ConvertTextureToAbgr8888(data));
        PspBootTrace::WriteLine(
            std::string("PspTextureCreateReady ptr=")
            + std::to_string(reinterpret_cast<std::uintptr_t>(runtimeTexture))
            + " pixelCount=" + std::to_string(runtimeTexture->GetPixelCount())
            + " freeMem=" + std::to_string(sceKernelTotalFreeMemSize()));
        return runtimeTexture;
    }

    /// Computes the expected cooked color-payload byte length for one texture asset.
    std::size_t PspTextureCache::GetExpectedColorByteCount(TextureAsset* data) {
        if (data == nullptr) {
            throw std::invalid_argument("PSP runtime texture data is required.");
        }

        const std::size_t pixelCount = static_cast<std::size_t>(data->Width) * static_cast<std::size_t>(data->Height);
        if (data->ColorFormat == TextureAssetColorFormat::Rgba32) {
            return pixelCount * 4u;
        } else if (data->ColorFormat == TextureAssetColorFormat::Rgba4444 || data->ColorFormat == TextureAssetColorFormat::GxRgb5A3) {
            return pixelCount * 2u;
        } else if (data->ColorFormat == TextureAssetColorFormat::Indexed4) {
            return (pixelCount + 1u) / 2u;
        } else if (data->ColorFormat == TextureAssetColorFormat::Indexed8) {
            return pixelCount;
        }

        throw std::runtime_error("PSP runtime texture color format is not supported.");
    }

    /// Converts one cooked texture payload into PSP-ready ABGR8888 texels.
    std::vector<std::uint32_t> PspTextureCache::ConvertTextureToAbgr8888(TextureAsset* data) {
        if (data == nullptr) {
            throw std::invalid_argument("PSP runtime texture data is required.");
        }

        const std::size_t pixelCount = static_cast<std::size_t>(data->Width) * static_cast<std::size_t>(data->Height);
        std::vector<std::uint32_t> pixels;
        pixels.reserve(pixelCount);

        if (data->ColorFormat == TextureAssetColorFormat::Rgba32) {
            for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
                const std::size_t byteOffset = pixelIndex * 4u;
                pixels.push_back(PackRgbaToAbgr(
                    static_cast<std::uint8_t>(data->Colors->Data[byteOffset]),
                    static_cast<std::uint8_t>(data->Colors->Data[byteOffset + 1u]),
                    static_cast<std::uint8_t>(data->Colors->Data[byteOffset + 2u]),
                    static_cast<std::uint8_t>(data->Colors->Data[byteOffset + 3u])));
            }

            return pixels;
        } else if (data->ColorFormat == TextureAssetColorFormat::Rgba4444) {
            for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
                const std::size_t byteOffset = pixelIndex * 2u;
                const std::uint16_t packedPixel = static_cast<std::uint16_t>(
                    static_cast<std::uint8_t>(data->Colors->Data[byteOffset])
                    | (static_cast<std::uint16_t>(static_cast<std::uint8_t>(data->Colors->Data[byteOffset + 1u])) << 8u));
                pixels.push_back(PackRgbaToAbgr(
                    ExpandChannelTo8Bit(static_cast<std::uint8_t>(packedPixel & 0x0Fu), 4),
                    ExpandChannelTo8Bit(static_cast<std::uint8_t>((packedPixel >> 4u) & 0x0Fu), 4),
                    ExpandChannelTo8Bit(static_cast<std::uint8_t>((packedPixel >> 8u) & 0x0Fu), 4),
                    ExpandChannelTo8Bit(static_cast<std::uint8_t>((packedPixel >> 12u) & 0x0Fu), 4)));
            }

            return pixels;
        } else if (data->ColorFormat == TextureAssetColorFormat::Indexed4) {
            for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
                const std::size_t byteOffset = pixelIndex / 2u;
                const std::uint8_t packedIndices = static_cast<std::uint8_t>(data->Colors->Data[byteOffset]);
                const std::size_t paletteIndex = (pixelIndex & 1u) == 0u
                    ? static_cast<std::size_t>(packedIndices & 0x0Fu)
                    : static_cast<std::size_t>((packedIndices >> 4u) & 0x0Fu);
                pixels.push_back(ReadIndexedPalettePixel(data, paletteIndex));
            }

            return pixels;
        } else if (data->ColorFormat == TextureAssetColorFormat::Indexed8) {
            for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
                const std::size_t paletteIndex = static_cast<std::size_t>(static_cast<std::uint8_t>(data->Colors->Data[pixelIndex]));
                pixels.push_back(ReadIndexedPalettePixel(data, paletteIndex));
            }

            return pixels;
        } else if (data->ColorFormat == TextureAssetColorFormat::GxRgb5A3) {
            for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
                const std::size_t byteOffset = pixelIndex * 2u;
                const std::uint16_t packedPixel = static_cast<std::uint16_t>(
                    static_cast<std::uint8_t>(data->Colors->Data[byteOffset])
                    | (static_cast<std::uint16_t>(static_cast<std::uint8_t>(data->Colors->Data[byteOffset + 1u])) << 8u));
                if ((packedPixel & 0x8000u) != 0u) {
                    pixels.push_back(PackRgbaToAbgr(
                        ExpandChannelTo8Bit(static_cast<std::uint8_t>((packedPixel >> 10u) & 0x1Fu), 5),
                        ExpandChannelTo8Bit(static_cast<std::uint8_t>((packedPixel >> 5u) & 0x1Fu), 5),
                        ExpandChannelTo8Bit(static_cast<std::uint8_t>(packedPixel & 0x1Fu), 5),
                        0xFFu));
                } else {
                    pixels.push_back(PackRgbaToAbgr(
                        ExpandChannelTo8Bit(static_cast<std::uint8_t>((packedPixel >> 8u) & 0x0Fu), 4),
                        ExpandChannelTo8Bit(static_cast<std::uint8_t>((packedPixel >> 4u) & 0x0Fu), 4),
                        ExpandChannelTo8Bit(static_cast<std::uint8_t>(packedPixel & 0x0Fu), 4),
                        ExpandChannelTo8Bit(static_cast<std::uint8_t>((packedPixel >> 12u) & 0x07u), 3)));
                }
            }

            return pixels;
        }

        throw std::runtime_error("PSP runtime texture color format is not supported.");
    }

    /// Converts one quantized channel back to 8-bit precision using the supplied bit count.
    std::uint8_t PspTextureCache::ExpandChannelTo8Bit(std::uint8_t value, int bitCount) {
        if (bitCount < 1 || bitCount > 8) {
            throw std::invalid_argument("PSP runtime texture channel bit count must be between 1 and 8.");
        } else if (bitCount == 8) {
            return value;
        }

        const std::uint32_t maximumInputValue = (1u << static_cast<std::uint32_t>(bitCount)) - 1u;
        return static_cast<std::uint8_t>((static_cast<std::uint32_t>(value) * 255u + (maximumInputValue / 2u)) / maximumInputValue);
    }

    /// Reads one palette-backed texel and packs it into PSP-ready ABGR8888 layout.
    std::uint32_t PspTextureCache::ReadIndexedPalettePixel(TextureAsset* data, std::size_t paletteIndex) {
        if (data == nullptr) {
            throw std::invalid_argument("PSP runtime texture data is required.");
        } else if (data->PaletteColors == nullptr || data->PaletteColors->Length == 0) {
            throw std::runtime_error("PSP indexed runtime textures must include palette colors.");
        }

        const std::size_t paletteEntryCount = static_cast<std::size_t>(data->PaletteColors->Length) / 4u;
        if (static_cast<std::size_t>(data->PaletteColors->Length) != paletteEntryCount * 4u) {
            throw std::runtime_error("PSP indexed runtime texture palette length must be a multiple of 4.");
        } else if (paletteIndex >= paletteEntryCount) {
            throw std::runtime_error("PSP indexed runtime texture palette index was outside the available palette.");
        }

        const std::size_t paletteOffset = paletteIndex * 4u;
        return PackRgbaToAbgr(
            static_cast<std::uint8_t>(data->PaletteColors->Data[paletteOffset]),
            static_cast<std::uint8_t>(data->PaletteColors->Data[paletteOffset + 1u]),
            static_cast<std::uint8_t>(data->PaletteColors->Data[paletteOffset + 2u]),
            static_cast<std::uint8_t>(data->PaletteColors->Data[paletteOffset + 3u]));
    }
}
