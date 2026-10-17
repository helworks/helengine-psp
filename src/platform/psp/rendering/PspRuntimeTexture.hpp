#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "RuntimeTexture.hpp"
#include "platform/psp/rendering/PspNativeTextureLayout.hpp"

namespace helengine::psp::rendering {
    /// Owns native GU texels and CLUT data, with an optional ABGR8888 CPU surface for font caching.
    class PspRuntimeTexture final : public RuntimeTexture {
    public:
        /// Creates an empty PSP runtime texture.
        PspRuntimeTexture();

        /// Releases one PSP runtime texture instance and emits focused diagnostics for native texture lifetime tracking.
        ~PspRuntimeTexture() override;

        /// Gets CPU ABGR8888 pixels, decoding native storage lazily when a font surface needs them.
        const std::uint32_t* GetPixelsAbgr8888() const;

        /// Gets the padded texel count, whether stored natively or expanded into a CPU surface.
        std::size_t GetPixelCount() const;

        /// Gets whether the texture owns pixel data.
        bool HasPixels() const;

        /// Gets the GU-facing texture buffer width used as the native row stride.
        std::uint16_t GetTextureBufferWidth() const;

        /// Assigns the GU-facing texture buffer width used as the native row stride.
        void SetTextureBufferWidth(std::uint16_t textureBufferWidth);

        /// Gets the power-of-two GU image height while inherited Height remains the authored UV extent.
        std::uint16_t GetTextureImageHeight() const;

        /// Assigns the power-of-two GU image height used for texture upload and binding.
        void SetTextureImageHeight(std::uint16_t textureImageHeight);

        /// Replaces the owned ABGR8888 pixel buffer.
        void SetPixelsAbgr8888(std::vector<std::uint32_t>&& pixels);

        /// Validates a mutable legacy RGBA region and rejects immutable native payloads before GPU synchronization.
        void ValidateRegionUpdate(int x, int y, int width, int height, const std::uint8_t* rgba8,
            std::size_t sourceBytes, int sourceRowPitch);

        /// Patches validated RGBA bytes in the retained allocation after the caller completes pending GU reads.
        void UpdateRegionFromRgba8(int x, int y, int width, int height,
            const std::uint8_t* rgba8, int sourceRowPitch);

        /// Transfers ownership of the ABGR8888 pixel buffer out of the runtime texture for deferred safe release.
        std::vector<std::uint32_t> TakePixelsAbgr8888();

        /// Gets the deterministic runtime asset id associated with this texture.
        std::uint64_t GetRuntimeAssetId() const;

        /// Assigns the deterministic runtime asset id associated with this texture.
        void SetRuntimeAssetId(std::uint64_t runtimeAssetId);

        /// Copies a checked native payload into 16-byte aligned owned storage before GU submission.
        void SetNativeData(const std::uint8_t* colors, std::size_t colorBytes, const std::uint8_t* palette, std::size_t paletteBytes, const PspNativeTextureLayout& layout);
        /// Gets the actual GPU texel and CLUT storage size, excluding the PGT1 header.
        std::size_t GetGpuByteCount() const;
        /// Writes back both texel and palette CPU caches before a native bind.
        void WritebackGpuData() const;
        /// Binds the exact GU format, storage order, palette and image for either renderer.
        void BindGpuData() const;
        /// Detaches native colors for release after the existing safe frame boundary.
        std::vector<std::uint32_t> TakeNativeColors();
        /// Detaches native CLUT data for release after the existing safe frame boundary.
        std::vector<std::uint32_t> TakeNativePalette();

    private:
        /// Stores legacy ABGR8888 texels or the lazily decoded native CPU font surface in row-major order.
        mutable std::vector<std::uint32_t> PixelsAbgr8888;
        /// Stores aligned PGT1 GPU texels independently of the optional CPU preview.
        std::vector<std::uint32_t> NativeColors;
        /// Stores the aligned native CLUT allocation.
        std::vector<std::uint32_t> NativePalette;
        /// Stores validated metadata associated with the native allocation.
        PspNativeTextureLayout NativeLayout;

        /// Finds a 16-byte aligned address within a word allocation padded by three words.
        static std::uint8_t* AlignedBytes(const std::vector<std::uint32_t>& storage);

        /// Stores the GU-facing texture buffer width used as the native row stride.
        std::uint16_t TextureBufferWidth = 0u;

        /// Stores the GU-facing power-of-two image height while inherited Height remains the authored UV extent.
        std::uint16_t TextureImageHeight = 0u;

        /// Stores the deterministic runtime asset id for cache reuse.
        std::uint64_t RuntimeAssetId = 0u;
    };
}
