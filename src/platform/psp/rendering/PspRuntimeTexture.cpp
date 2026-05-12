#include "platform/psp/rendering/PspRuntimeTexture.hpp"

#include <utility>

namespace helengine::psp::rendering {
    /// Creates an empty PSP runtime texture.
    PspRuntimeTexture::PspRuntimeTexture() = default;

    /// Gets the PSP-ready ABGR8888 texel pointer, or <c>nullptr</c> when the texture has no pixels.
    const std::uint32_t* PspRuntimeTexture::GetPixelsAbgr8888() const {
        return PixelsAbgr8888.empty()
            ? nullptr
            : PixelsAbgr8888.data();
    }

    /// Gets the number of stored ABGR8888 texels.
    std::size_t PspRuntimeTexture::GetPixelCount() const {
        return PixelsAbgr8888.size();
    }

    /// Gets whether the texture owns pixel data.
    bool PspRuntimeTexture::HasPixels() const {
        return !PixelsAbgr8888.empty();
    }

    /// Replaces the owned ABGR8888 pixel buffer.
    void PspRuntimeTexture::SetPixelsAbgr8888(std::vector<std::uint32_t>&& pixels) {
        PixelsAbgr8888 = std::move(pixels);
    }
}
