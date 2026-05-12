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

        /// Gets the PSP-ready ABGR8888 texel pointer, or <c>nullptr</c> when the texture has no pixels.
        const std::uint32_t* GetPixelsAbgr8888() const;

        /// Gets the number of stored ABGR8888 texels.
        std::size_t GetPixelCount() const;

        /// Gets whether the texture owns pixel data.
        bool HasPixels() const;

        /// Replaces the owned ABGR8888 pixel buffer.
        void SetPixelsAbgr8888(std::vector<std::uint32_t>&& pixels);

    private:
        /// Stores PSP-ready ABGR8888 texels in row-major order.
        std::vector<std::uint32_t> PixelsAbgr8888;
    };
}
