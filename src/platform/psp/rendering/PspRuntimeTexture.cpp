#include "platform/psp/rendering/PspRuntimeTexture.hpp"

#include <cstdint>
#include <utility>
#include <cstring>
#include <pspgu.h>
#include <pspkernel.h>
#include "platform/psp/rendering/PspNativeTexture.hpp"

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

    /// Gets CPU ABGR8888 pixels, decoding native storage lazily when a font surface needs them.
    const std::uint32_t* PspRuntimeTexture::GetPixelsAbgr8888() const {
        if (PixelsAbgr8888.empty() && !NativeColors.empty()) PixelsAbgr8888 = PspNativeTexture::Decode(AlignedBytes(NativeColors), AlignedBytes(NativePalette), NativeLayout);
        return PixelsAbgr8888.empty()
            ? nullptr
            : PixelsAbgr8888.data();
    }

    /// Gets the padded texel count independently of whether a CPU preview has been allocated.
    std::size_t PspRuntimeTexture::GetPixelCount() const {
        if (!NativeColors.empty()) return NativeLayout.StorageWidth * NativeLayout.StorageHeight;
        return PixelsAbgr8888.size();
    }

    /// Gets whether the texture owns pixel data.
    bool PspRuntimeTexture::HasPixels() const {
        return !PixelsAbgr8888.empty() || !NativeColors.empty();
    }

    /// Gets the GU-facing texture buffer width used as the native row stride.
    std::uint16_t PspRuntimeTexture::GetTextureBufferWidth() const {
        return TextureBufferWidth;
    }

    /// Assigns the GU-facing texture buffer width used as the native row stride.
    void PspRuntimeTexture::SetTextureBufferWidth(std::uint16_t textureBufferWidth) {
        TextureBufferWidth = textureBufferWidth;
    }

    /// Gets the power-of-two GU image height while inherited Height remains the authored UV extent.
    std::uint16_t PspRuntimeTexture::GetTextureImageHeight() const {
        return TextureImageHeight;
    }

    /// Assigns the power-of-two GU image height used for texture upload and binding.
    void PspRuntimeTexture::SetTextureImageHeight(std::uint16_t textureImageHeight) {
        TextureImageHeight = textureImageHeight;
    }

    /// Replaces the owned ABGR8888 pixel buffer.
    void PspRuntimeTexture::SetPixelsAbgr8888(std::vector<std::uint32_t>&& pixels) {
        PixelsAbgr8888 = std::move(pixels);
    }

    /// Transfers the owned ABGR8888 pixel buffer out of the runtime texture for deferred safe release.
    std::vector<std::uint32_t> PspRuntimeTexture::TakePixelsAbgr8888() {
        return std::move(PixelsAbgr8888);
    }

    /// Finds a 16-byte aligned subrange inside storage that reserved three extra words.
    std::uint8_t* PspRuntimeTexture::AlignedBytes(const std::vector<std::uint32_t>& storage) {
        if (storage.empty()) return nullptr;
        return reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 15u) & ~std::uintptr_t(15u));
    }
    /// Allocates both native buffers before publishing the texture metadata.
    void PspRuntimeTexture::SetNativeData(const std::uint8_t* colors, std::size_t colorBytes, const std::uint8_t* palette, std::size_t paletteBytes, const PspNativeTextureLayout& layout) {
        std::vector<std::uint32_t> copiedColors((colorBytes + 3) / 4 + 3);
        std::vector<std::uint32_t> copiedPalette(paletteBytes == 0 ? 0 : (paletteBytes + 3) / 4 + 3);
        std::memcpy(AlignedBytes(copiedColors), colors, colorBytes);
        if (paletteBytes != 0) std::memcpy(AlignedBytes(copiedPalette), palette, paletteBytes);
        NativeColors = std::move(copiedColors); NativePalette = std::move(copiedPalette); NativeLayout = layout;
        PixelsAbgr8888.clear();
        SetTextureBufferWidth(static_cast<std::uint16_t>(layout.StorageWidth));
        SetTextureImageHeight(static_cast<std::uint16_t>(layout.StorageHeight));
    }
    /// Reports GPU memory independently of any lazily generated font preview.
    std::size_t PspRuntimeTexture::GetGpuByteCount() const {
        return NativeColors.empty() ? PixelsAbgr8888.size() * 4 : NativeLayout.TexelBytes + NativeLayout.PaletteBytes;
    }
    /// Flushes exactly the GPU-visible texel and palette ranges.
    void PspRuntimeTexture::WritebackGpuData() const {
        if (NativeColors.empty()) sceKernelDcacheWritebackRange(const_cast<std::uint32_t*>(PixelsAbgr8888.data()), static_cast<unsigned int>(PixelsAbgr8888.size() * 4));
        else {
            sceKernelDcacheWritebackRange(AlignedBytes(NativeColors) + 48, static_cast<unsigned int>(NativeLayout.TexelBytes));
            if (NativeLayout.PaletteBytes != 0) sceKernelDcacheWritebackRange(AlignedBytes(NativePalette), static_cast<unsigned int>(NativeLayout.PaletteBytes));
        }
    }
    /// Submits native formats directly and resets the GU texel cache at every changed texture bind.
    void PspRuntimeTexture::BindGpuData() const {
        const bool native = !NativeColors.empty();
        sceGuTexMode(native ? static_cast<int>(NativeLayout.Format) : GU_PSM_8888, 0, 0, native && NativeLayout.Swizzled ? 1 : 0);
        if (native && NativeLayout.PaletteEntries != 0) {
            sceGuClutMode(static_cast<int>(NativeLayout.PaletteFormat), 0, NativeLayout.PaletteEntries == 16 ? 15 : 255, 0);
            sceGuClutLoad(static_cast<int>(NativeLayout.PaletteBytes / 32), AlignedBytes(NativePalette));
        }
        sceGuTexImage(0, GetTextureBufferWidth(), GetTextureImageHeight(), GetTextureBufferWidth(), native ? static_cast<const void*>(AlignedBytes(NativeColors) + 48) : static_cast<const void*>(PixelsAbgr8888.data()));
        sceGuTexFlush();
    }
    /// Retains the aligned texel allocation until the renderer synchronizes pending GPU reads.
    std::vector<std::uint32_t> PspRuntimeTexture::TakeNativeColors() { return std::move(NativeColors); }
    /// Retains the aligned CLUT allocation until the renderer synchronizes pending GPU reads.
    std::vector<std::uint32_t> PspRuntimeTexture::TakeNativePalette() { return std::move(NativePalette); }

    /// Gets the deterministic runtime asset id associated with this texture.
    std::uint64_t PspRuntimeTexture::GetRuntimeAssetId() const {
        return RuntimeAssetId;
    }

    /// Assigns the deterministic runtime asset id associated with this texture.
    void PspRuntimeTexture::SetRuntimeAssetId(std::uint64_t runtimeAssetId) {
        RuntimeAssetId = runtimeAssetId;
    }
}
