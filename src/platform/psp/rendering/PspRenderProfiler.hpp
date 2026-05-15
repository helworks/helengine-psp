#pragma once

#include <cstddef>
#include <cstdint>

#include "int2.hpp"

namespace helengine::psp::rendering {
    class PspRuntimeTexture;

    /// Captures bounded per-frame renderer timings and emits them into the PSP boot trace for performance investigations.
    class PspRenderProfiler final {
    public:
        /// Returns the current PSP system timestamp in microseconds.
        static std::uint64_t GetTimestampMicroseconds();

        /// Starts a new renderer frame capture.
        static void BeginFrame();

        /// Completes the current renderer frame capture and emits any configured trace output.
        static void EndFrame();

        /// Records one 2D texture bind, including total bind time and the texture writeback portion.
        static void Record2DTextureBind(PspRuntimeTexture* texture, std::size_t byteCount, std::uint64_t totalMicroseconds, std::uint64_t flushMicroseconds);

        /// Records one 3D texture bind, including total bind time and the texture writeback portion.
        static void Record3DTextureBind(PspRuntimeTexture* texture, std::size_t byteCount, std::uint64_t totalMicroseconds, std::uint64_t flushMicroseconds);

        /// Records one 2D textured-quad draw.
        static void Record2DTexturedQuad(PspRuntimeTexture* texture, const int2& size, std::uint64_t totalMicroseconds, std::uint64_t drawMicroseconds);

        /// Records one 2D textured-triangle draw.
        static void Record2DTexturedTriangles(PspRuntimeTexture* texture, std::size_t vertexCount, std::uint64_t totalMicroseconds, std::uint64_t drawMicroseconds);

        /// Records one 2D text draw and the amount of visible glyph work it submitted.
        static void Record2DText(int32_t glyphCount, int32_t contentLength, std::uint64_t totalMicroseconds);

        /// Records one complete 2D camera pass.
        static void Record2DCamera(int32_t drawableCount, std::uint64_t totalMicroseconds);

        /// Records one complete 3D camera pass and the time spent in its nested 2D UI phase.
        static void Record3DCamera(int32_t drawableCount, std::uint64_t totalMicroseconds, std::uint64_t uiMicroseconds);
    };
}
