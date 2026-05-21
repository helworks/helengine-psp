#include "platform/psp/rendering/PspRuntimeTexture.hpp"

#include <cstdint>
#include <utility>

#include "platform/psp/PspBootTrace.hpp"

namespace helengine::psp::rendering {
    /// Creates an empty PSP runtime texture.
    PspRuntimeTexture::PspRuntimeTexture() = default;

    /// Releases one PSP runtime texture instance and emits focused diagnostics for native texture lifetime tracking.
    PspRuntimeTexture::~PspRuntimeTexture() {
        PspBootTrace::WriteLine(
            std::string("PspRuntimeTextureDestroy ptr=")
            + std::to_string(reinterpret_cast<std::uintptr_t>(this))
            + " id=" + get_Id()
            + " width=" + std::to_string(get_Width())
            + " height=" + std::to_string(get_Height())
            + " pixels=" + std::to_string(PixelsAbgr8888.size()));
    }

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

    /// Transfers the owned ABGR8888 pixel buffer out of the runtime texture for deferred safe release.
    std::vector<std::uint32_t> PspRuntimeTexture::TakePixelsAbgr8888() {
        return std::move(PixelsAbgr8888);
    }

    /// Gets the deterministic runtime asset id associated with this texture.
    std::uint64_t PspRuntimeTexture::GetRuntimeAssetId() const {
        return RuntimeAssetId;
    }

    /// Assigns the deterministic runtime asset id associated with this texture.
    void PspRuntimeTexture::SetRuntimeAssetId(std::uint64_t runtimeAssetId) {
        RuntimeAssetId = runtimeAssetId;
    }
}
