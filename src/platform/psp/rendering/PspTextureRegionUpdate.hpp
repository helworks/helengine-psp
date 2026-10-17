#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace helengine::psp::rendering {
    /// Validates and patches retained ABGR8888 storage without moving its GPU-visible allocation.
    class PspTextureRegionUpdate final {
    public:
        /// Rejects invalid source rows or logical bounds before any destination byte changes.
        static void Validate(int logicalWidth, int logicalHeight, std::size_t destinationStride,
            std::size_t destinationPixels, int x, int y, int width, int height,
            const std::uint8_t* rgba8, std::size_t sourceBytes, int sourceRowPitch) {
            if (logicalWidth <= 0 || logicalHeight <= 0 || width <= 0 || height <= 0
                || x < 0 || y < 0 || width > logicalWidth || height > logicalHeight
                || x > logicalWidth - width || y > logicalHeight - height) {
                throw std::invalid_argument("PSP texture update exceeds logical texture bounds.");
            }
            if (destinationStride < static_cast<std::size_t>(logicalWidth)
                || destinationPixels / destinationStride < static_cast<std::size_t>(logicalHeight)) {
                throw std::invalid_argument("PSP texture update requires complete retained RGBA storage.");
            }
            const std::uint64_t rowBytes = static_cast<std::uint64_t>(width) * 4u;
            if (rgba8 == nullptr || sourceRowPitch <= 0 || static_cast<std::uint64_t>(sourceRowPitch) < rowBytes) {
                throw std::invalid_argument("PSP texture update source row pitch is invalid.");
            }
            const std::uint64_t required = static_cast<std::uint64_t>(height - 1)
                * static_cast<std::uint64_t>(sourceRowPitch) + rowBytes;
            if (required > sourceBytes) {
                throw std::invalid_argument("PSP texture update source rows are truncated.");
            }
        }

        /// Copies validated RGBA8 rows to the existing padded ABGR8888 word buffer, preserving row padding.
        static void Copy(std::uint32_t* destination, std::size_t destinationStride,
            int x, int y, int width, int height, const std::uint8_t* rgba8, int sourceRowPitch) {
            for (int row = 0; row < height; ++row) {
                const std::uint8_t* source = rgba8 + static_cast<std::size_t>(row) * sourceRowPitch;
                std::uint32_t* target = destination + static_cast<std::size_t>(y + row) * destinationStride + x;
                for (int column = 0; column < width; ++column) {
                    const std::uint8_t* pixel = source + static_cast<std::size_t>(column) * 4u;
                    target[column] = static_cast<std::uint32_t>(pixel[0])
                        | (static_cast<std::uint32_t>(pixel[1]) << 8)
                        | (static_cast<std::uint32_t>(pixel[2]) << 16)
                        | (static_cast<std::uint32_t>(pixel[3]) << 24);
                }
            }
        }
    };
}
