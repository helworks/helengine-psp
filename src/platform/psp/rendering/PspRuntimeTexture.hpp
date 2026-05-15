#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "RuntimeTexture.hpp"

namespace helengine::psp::rendering {
    /// Stores PSP-ready ABGR8888 texture pixels that can be bound directly through GU.
    class PspRuntimeTexture final : public RuntimeTexture {
    public:
        /// Creates an empty PSP runtime texture.
        PspRuntimeTexture();

        /// Releases one PSP runtime texture instance and emits focused diagnostics for native texture lifetime tracking.
        ~PspRuntimeTexture() override;

        /// Gets the PSP-ready ABGR8888 texel pointer, or <c>nullptr</c> when the texture has no pixels.
        const std::uint32_t* GetPixelsAbgr8888() const;

        /// Gets the number of stored ABGR8888 texels.
        std::size_t GetPixelCount() const;

        /// Gets whether the texture owns pixel data.
        bool HasPixels() const;

        /// Replaces the owned ABGR8888 pixel buffer.
        void SetPixelsAbgr8888(std::vector<std::uint32_t>&& pixels);

        /// Gets the deterministic runtime asset id associated with this texture.
        std::uint64_t GetRuntimeAssetId() const;

        /// Assigns the deterministic runtime asset id associated with this texture.
        void SetRuntimeAssetId(std::uint64_t runtimeAssetId);

    private:
        /// Stores PSP-ready ABGR8888 texels in row-major order.
        std::vector<std::uint32_t> PixelsAbgr8888;

        /// Stores the deterministic runtime asset id for cache reuse.
        std::uint64_t RuntimeAssetId = 0u;
    };
}
