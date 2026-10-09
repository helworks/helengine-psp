#include "platform/psp/rendering/PspNativeTexture.hpp"
#include <stdexcept>

namespace helengine::psp::rendering {
    namespace {
        /// Reads a little-endian native word after the caller has established its byte bounds.
        std::uint32_t ReadWord(const std::uint8_t* data, std::size_t offset, int bytes) {
            std::uint32_t value = 0;
            for (int index = 0; index < bytes; index++) value |= static_cast<std::uint32_t>(data[offset + index]) << (index * 8);
            return value;
        }
        /// Gets bits per component texel or average bits per compressed texel.
        std::uint32_t Bits(std::uint32_t code) {
            return code == 4 || code == 8 ? 4 : code == 5 || code >= 9 ? 8 : code == 3 || code == 7 ? 32 : 16;
        }
        /// Rounds an extent to the canonical GU power-of-two storage size.
        std::uint32_t PowerOfTwo(std::uint32_t value, std::uint32_t minimum) {
            while (minimum < value) minimum *= 2;
            return minimum;
        }
        /// Expands one low-bit channel using native high-bit replication.
        std::uint32_t Expand(std::uint32_t value, int bits) {
            const std::uint32_t mask = (1u << bits) - 1;
            const std::uint32_t channel = value & mask;
            return bits == 5 ? channel << 3 | channel >> 2 : bits == 6 ? channel << 2 | channel >> 4 : bits == 4 ? channel * 17 : channel;
        }
        /// Converts PSP packed direct color to ABGR8888 CPU memory order.
        std::uint32_t Unpack(std::uint32_t value, std::uint32_t code) {
            const int redBits = code <= 1 ? 5 : code == 2 ? 4 : 8;
            const int greenBits = code == 0 ? 6 : redBits;
            const std::uint32_t alpha = code == 0 ? 255 : code == 1 ? (value >> 15) * 255 : Expand(value >> (redBits * 2 + greenBits), code == 2 ? 4 : 8);
            return Expand(value, redBits) | Expand(value >> redBits, greenBits) << 8 | Expand(value >> (redBits + greenBits), redBits) << 16 | alpha << 24;
        }
        /// Resolves a native uncompressed row byte in linear or PSP faster order.
        std::size_t ByteOffset(std::uint32_t xByte, std::uint32_t y, const PspNativeTextureLayout& layout) {
            const std::uint32_t pitch = layout.StorageWidth * Bits(layout.Format) / 8;
            return layout.Swizzled ? (y / 8 * (pitch / 16) + xByte / 16) * 128 + y % 8 * 16 + xByte % 16 : y * pitch + xByte;
        }
        /// Reads a canonical palette index without masking malformed upper index bits.
        std::uint32_t ReadIndex(const std::uint8_t* pixels, std::uint32_t x, std::uint32_t y, const PspNativeTextureLayout& layout) {
            const std::size_t offset = ByteOffset(x * Bits(layout.Format) / 8, y, layout);
            return layout.Format == 4 ? pixels[offset] >> (x % 2 * 4) & 15 : ReadWord(pixels, offset, Bits(layout.Format) / 8);
        }
        /// Decodes the standard RGB565 endpoints stored after PSP color selectors.
        std::uint32_t DxtColor(std::uint32_t value) {
            return Expand(value >> 11, 5) | Expand(value >> 5, 6) << 8 | Expand(value, 5) << 16 | 0xff000000u;
        }
        /// Interpolates a DXT color using the hardware's integer channel arithmetic.
        std::uint32_t Mix(std::uint32_t a, std::uint32_t b, int firstWeight, int secondWeight, int divisor) {
            std::uint32_t result = 0xff000000u;
            for (int shift = 0; shift < 24; shift += 8) result |= ((((a >> shift) & 255) * firstWeight + ((b >> shift) & 255) * secondWeight) / divisor) << shift;
            return result;
        }
        /// Samples a PSP-ordered DXT block, including explicit or interpolated alpha stored after color.
        std::uint32_t SampleDxt(const std::uint8_t* block, std::uint32_t code, int pixel) {
            const std::uint32_t first = ReadWord(block, 4, 2), second = ReadWord(block, 6, 2);
            const std::uint32_t a = DxtColor(first), b = DxtColor(second);
            std::uint32_t colors[4] = { a, b, 0, 0 };
            if (code == 8 && first <= second) { colors[2] = Mix(a, b, 1, 1, 2); colors[3] = 0; }
            else { colors[2] = Mix(a, b, 2, 1, 3); colors[3] = Mix(a, b, 1, 2, 3); }
            std::uint32_t color = colors[(ReadWord(block, 0, 4) >> (pixel * 2)) & 3];
            std::uint32_t alpha = color >> 24;
            if (code == 9) alpha = ((block[8 + pixel / 2] >> (pixel % 2 * 4)) & 15) * 17;
            else if (code == 10) {
                const std::uint32_t firstAlpha = block[14], secondAlpha = block[15];
                std::uint32_t alphas[8] = { firstAlpha, secondAlpha, 0, 0, 0, 0, 0, 0 };
                if (firstAlpha > secondAlpha) {
                    for (int index = 1; index <= 6; index++) alphas[index + 1] = ((7 - index) * firstAlpha + index * secondAlpha) / 7;
                } else {
                    for (int index = 1; index <= 4; index++) alphas[index + 1] = ((5 - index) * firstAlpha + index * secondAlpha) / 5;
                    alphas[6] = 0; alphas[7] = 255;
                }
                const std::uint64_t indices = static_cast<std::uint64_t>(ReadWord(block, 12, 2)) | static_cast<std::uint64_t>(ReadWord(block, 8, 4)) << 16;
                alpha = alphas[(indices >> (pixel * 3)) & 7];
            }
            return (color & 0x00ffffffu) | alpha << 24;
        }
    }
    /// Checks every length and native control before trusting offsets for GU submission.
    PspNativeTextureLayout PspNativeTexture::Read(const std::uint8_t* colors, std::size_t colorBytes, std::size_t paletteBytes, std::uint32_t width, std::uint32_t height) {
        if (colors == nullptr || colorBytes < 48 || ReadWord(colors, 0, 4) != 0x31544750u || ReadWord(colors, 4, 4) != 1) throw std::runtime_error("Invalid PSP PGT1 texture header.");
        PspNativeTextureLayout layout;
        layout.Format = ReadWord(colors, 8, 4);
        const std::uint32_t flags = ReadWord(colors, 12, 4);
        layout.PaletteFormat = ReadWord(colors, 36, 4);
        if (width < 1 || height < 1 || width > 512 || height > 512 || layout.Format > 10 || flags > 1 || layout.PaletteFormat > 3
            || (layout.Format >= 8 && flags != 0) || ((layout.Format < 4 || layout.Format >= 8) && layout.PaletteFormat != 0)) throw std::runtime_error("Unsupported PSP texture metadata.");
        layout.Width = width; layout.Height = height; layout.Swizzled = flags != 0;
        layout.StorageWidth = PowerOfTwo(width, layout.Format >= 8 ? 4 : 128 / Bits(layout.Format));
        layout.StorageHeight = PowerOfTwo(height, layout.Format >= 8 ? 4 : layout.Swizzled ? 8 : 1);
        layout.TexelBytes = layout.StorageWidth * layout.StorageHeight * Bits(layout.Format) / 8;
        layout.PaletteEntries = layout.Format >= 4 && layout.Format <= 7 ? layout.Format == 4 ? 16 : 256 : 0;
        layout.PaletteBytes = layout.PaletteEntries * (layout.PaletteFormat == 3 ? 4 : 2);
        if (ReadWord(colors, 16, 4) != width || ReadWord(colors, 20, 4) != height || ReadWord(colors, 24, 4) != layout.StorageWidth
            || ReadWord(colors, 28, 4) != layout.StorageHeight || ReadWord(colors, 32, 4) != layout.TexelBytes
            || ReadWord(colors, 40, 4) != layout.PaletteEntries || ReadWord(colors, 44, 4) != layout.PaletteBytes
            || colorBytes != 48 + layout.TexelBytes || paletteBytes != layout.PaletteBytes) throw std::runtime_error("Malformed PSP native texture bounds or palette.");
        if (layout.PaletteEntries != 0) {
            for (std::uint32_t y = 0; y < layout.StorageHeight; y++) {
                for (std::uint32_t x = 0; x < layout.StorageWidth; x++) {
                    if (ReadIndex(colors + 48, x, y, layout) >= layout.PaletteEntries) throw std::runtime_error("PSP CLUT index outside the canonical palette.");
                }
            }
        }
        return layout;
    }
    /// Expands native storage only for a CPU font surface; GPU draws retain the original encoded bytes.
    std::vector<std::uint32_t> PspNativeTexture::Decode(const std::uint8_t* colors, const std::uint8_t* palette, const PspNativeTextureLayout& layout) {
        std::vector<std::uint32_t> pixels(layout.StorageWidth * layout.StorageHeight);
        const std::uint8_t* texels = colors + 48;
        for (std::uint32_t y = 0; y < layout.StorageHeight; y++) {
            for (std::uint32_t x = 0; x < layout.StorageWidth; x++) {
                std::uint32_t value;
                if (layout.Format >= 8) {
                    const std::size_t block = (y / 4 * (layout.StorageWidth / 4) + x / 4) * (layout.Format == 8 ? 8 : 16);
                    value = SampleDxt(texels + block, layout.Format, y % 4 * 4 + x % 4);
                } else if (layout.PaletteEntries != 0) {
                    const int entryBytes = layout.PaletteFormat == 3 ? 4 : 2;
                    value = Unpack(ReadWord(palette, ReadIndex(texels, x, y, layout) * entryBytes, entryBytes), layout.PaletteFormat);
                } else value = Unpack(ReadWord(texels, ByteOffset(x * Bits(layout.Format) / 8, y, layout), Bits(layout.Format) / 8), layout.Format);
                pixels[y * layout.StorageWidth + x] = value;
            }
        }
        return pixels;
    }
}
