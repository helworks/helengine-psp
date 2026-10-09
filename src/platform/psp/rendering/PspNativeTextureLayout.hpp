#pragma once
#include <cstddef>
#include <cstdint>

namespace helengine::psp::rendering {
    /// Stores checked single-level GU metadata; no pointers survive validation implicitly.
    struct PspNativeTextureLayout final {
        /// Stores the GU_PSM code in the range zero through ten.
        std::uint32_t Format = 0;
        /// Stores the verified uncompressed 16-byte by 8-row swizzle flag.
        bool Swizzled = false;
        /// Stores authored UV width.
        std::uint32_t Width = 0;
        /// Stores authored UV height.
        std::uint32_t Height = 0;
        /// Stores the GU power-of-two width and buffer stride.
        std::uint32_t StorageWidth = 0;
        /// Stores the GU power-of-two height.
        std::uint32_t StorageHeight = 0;
        /// Stores the native texel allocation length.
        std::size_t TexelBytes = 0;
        /// Stores the native direct-color CLUT format.
        std::uint32_t PaletteFormat = 0;
        /// Stores the canonical 16- or 256-entry CLUT budget.
        std::uint32_t PaletteEntries = 0;
        /// Stores the native CLUT byte length.
        std::size_t PaletteBytes = 0;
    };
}
