#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ICamera.hpp"
#include "ClipRegionStackBuilder2D.hpp"
#include "IDrawable2D.hpp"
#include "IClipRegion2D.hpp"
#include "IRenderVisitor2D.hpp"
#include "RenderManager2D.hpp"
#include "RuntimeTexture.hpp"
#include "TextureAsset.hpp"
#include "byte4.hpp"
#include "float4.hpp"
#include "float3.hpp"
#include "int2.hpp"
#include "runtime/native_list.hpp"
#include "platform/psp/rendering/PspRenderProfiler.hpp"
#include "platform/psp/rendering/PspRuntimeTexture.hpp"
#include "platform/psp/rendering/PspTextureCache.hpp"

namespace helengine::psp::rendering {
    /// Provides the PSP 2D runtime surface used by menus and other simple UI primitives.
    class PspRenderManager2D final : public RenderManager2D, public IRenderVisitor2D {
    public:
        /// Builds a PSP runtime texture from raw texture metadata.
        RuntimeTexture* BuildTextureFromRaw(TextureAsset* data) override;

        /// Releases one PSP runtime texture previously created by this renderer.
        void ReleaseTexture(RuntimeTexture* texture) override;

        /// Releases one PSP font asset and its native-owned object graph.
        void ReleaseFont(FontAsset* font) override;

        /// Flushes deferred PSP runtime texture deletions once the renderer reaches a safe boundary before the next scene begins loading.
        void FlushReleasedTextures() override;

        /// Renders one camera's queued 2D drawables in authored order.
        void RenderCamera(ICamera* camera);

        /// Draws a rounded rectangle using a PSP-friendly rectangular fallback.
        void DrawRoundedRect(IRoundedRectDrawable2D* shape) override;

        /// Draws a textured or solid sprite quad.
        void DrawSprite(ISpriteDrawable2D* sprite) override;

        /// Draws bitmap-font text using glyph quads.
        void DrawText(ITextDrawable2D* text) override;

        /// Visits one queued 2D drawable and dispatches it back through generated-core.
        void Visit(IDrawable2D* drawable) override;

    private:
        /// Stores one GU 2D vertex with UVs, packed color, and screen-space position.
        struct Psp2DVertex {
            float U;
            float V;
            std::uint32_t Color;
            float X;
            float Y;
            float Z;
        };

        /// Stores cached text geometry and the authored inputs used to build it.
        struct TextGeometryCacheEntry {
            /// Stores the drawable instance that owns this cached text geometry.
            ITextDrawable2D* Drawable = nullptr;

            /// Stores the font texture used when the cached geometry was built.
            RuntimeTexture* Texture = nullptr;

            /// Stores the authored text content after wrapping has been applied.
            std::string Content;

            /// Stores the authored text content before wrapping so cache invalidation can track source changes correctly.
            std::string RawText;

            /// Stores the parent position baked into the cached vertices.
            float3 Position = float3();

            /// Stores the text layout size used for wrapping.
            int2 Size = int2();

            /// Stores the glyph tint baked into the cached vertices.
            byte4 Color = byte4();

            /// Stores whether wrapping was enabled when the cache was built.
            bool WrapText = false;

            /// Stores the glyph scale used when the cache was built.
            float FontScale = 0.0f;

            /// Stores the number of visible glyphs represented by the cached geometry.
            int32_t GlyphCount = 0;

            /// Stores whether this cached entry should render through a pre-rendered static text surface.
            bool UsesStaticSurface = false;

            /// Stores the pre-rendered PSP runtime texture used for static text surfaces.
            PspRuntimeTexture* StaticSurfaceTexture = nullptr;

            /// Stores the screen-space draw position for the static text surface.
            float3 StaticSurfacePosition = float3();

            /// Stores the tight size of the static text surface.
            int2 StaticSurfaceSize = int2();

            /// Stores the cached textured triangle vertices for this text block.
            std::vector<Psp2DVertex> Vertices;
        };

        /// Stores cached rounded-rectangle geometry and the authored inputs used to build it.
        struct RoundedRectGeometryCacheEntry {
            /// Stores the drawable instance that owns this cached rounded-rectangle geometry.
            IRoundedRectDrawable2D* Drawable = nullptr;

            /// Stores the parent position baked into the cached border geometry.
            float3 Position = float3();

            /// Stores the destination size used when the cache was built.
            int2 Size = int2();

            /// Stores the rounded-corner radius used when the cache was built.
            float Radius = 0.0f;

            /// Stores the border thickness used when the cache was built.
            float BorderThickness = 0.0f;

            /// Stores the fill color used when the cache was built.
            byte4 FillColor = byte4();

            /// Stores the border color used when the cache was built.
            byte4 BorderColor = byte4();

            /// Stores whether the border geometry should be emitted.
            bool HasBorder = false;

            /// Stores whether the inner fill geometry should be emitted.
            bool HasInnerFill = false;

            /// Stores whether the border falls back to one simple rectangular quad.
            bool BorderUsesSolidQuad = false;

            /// Stores whether the inner fill falls back to one simple rectangular quad.
            bool InnerFillUsesSolidQuad = false;

            /// Stores the outer border position for solid-quad or rounded geometry emission.
            float3 BorderPosition = float3();

            /// Stores the inner fill position for solid-quad or rounded geometry emission.
            float3 InnerFillPosition = float3();

            /// Stores the outer border size for solid-quad or rounded geometry emission.
            int2 BorderSize = int2();

            /// Stores the inner fill size for solid-quad or rounded geometry emission.
            int2 InnerFillSize = int2();

            /// Stores the outer border radius for rounded geometry emission.
            float BorderRadius = 0.0f;

            /// Stores the inner fill radius for rounded geometry emission.
            float InnerFillRadius = 0.0f;

            /// Stores the cached textured white-triangle geometry for the outer border.
            std::vector<Psp2DVertex> BorderVertices;

            /// Stores the cached textured white-triangle geometry for the inner fill.
            std::vector<Psp2DVertex> InnerFillVertices;
        };

        /// Stores cached PSP runtime textures built from cooked texture assets.
        PspTextureCache TextureCache;

        /// Builds the active clip chain for one drawable using the generated-core clip-region contract.
        ClipRegionStackBuilder2D ClipRegionStackBuilder;

        /// Stores the reusable clip-chain list used while resolving one drawable's effective clip rectangle.
        List<IClipRegion2D*> ClipChain;

        /// Stores the reusable white runtime texture used to render solid 2D quads through the proven textured path.
        PspRuntimeTexture* WhiteTexture = nullptr;

        /// Stores the texture currently bound for the active 2D camera pass so redundant GU texture binds can be skipped.
        PspRuntimeTexture* BoundTexture = nullptr;

        /// Stores whether one drawable-scoped clip rectangle is currently active on the PSP GU.
        bool HasActiveClipRect = false;

        /// Stores the current drawable-scoped clip rectangle so redundant GU state changes can be skipped.
        float4 ActiveClipRect = float4();

        /// Stores cached text geometry for static PSP UI drawables.
        std::vector<TextGeometryCacheEntry> TextGeometryCacheEntries;

        /// Stores cached rounded-rectangle geometry for static PSP UI drawables.
        std::vector<RoundedRectGeometryCacheEntry> RoundedRectGeometryCacheEntries;

        /// Stores one pending batch of white-texture UI triangles that can be submitted together.
        std::vector<Psp2DVertex> PendingWhiteTriangles;

        /// Draws one solid-colored screen-space quad.
        void DrawSolidQuad(const float3& position, const int2& size, const byte4& color);

        /// Binds one PSP runtime texture for GU sampling or disables texturing when no texture exists.
        void BindTexture(PspRuntimeTexture* texture);

        /// Clears any cached bound-texture state at camera-pass boundaries.
        void ResetBoundTextureState();

        /// Draws one textured screen-space quad using normalized UV coordinates.
        void DrawTexturedQuad(RuntimeTexture* texture, const float3& position, const int2& size, const float4& sourceRect, const byte4& color);

        /// Draws one filled rounded rectangle using textured white geometry and triangle fans for the corners.
        void DrawFilledRoundedRect(const float3& position, const int2& size, float radius, const byte4& color);

        /// Draws one cached rounded rectangle using the supplied cached geometry entry.
        void DrawCachedRoundedRectGeometry(const RoundedRectGeometryCacheEntry& entry);

        /// Appends one solid-colored quad to the pending white-texture batch.
        void AppendSolidQuadToWhiteBatch(const float3& position, const int2& size, const byte4& color);

        /// Appends one list of white-texture triangles to the pending batch.
        void AppendWhiteTrianglesToBatch(const std::vector<Psp2DVertex>& vertices);

        /// Flushes any pending white-texture batch before state changes or frame completion.
        void FlushPendingWhiteTriangles();

        /// Finds or rebuilds the cached text geometry for one drawable.
        TextGeometryCacheEntry& GetOrBuildTextGeometryCacheEntry(ITextDrawable2D* text);

        /// Finds or rebuilds the cached rounded-rectangle geometry for one drawable.
        RoundedRectGeometryCacheEntry& GetOrBuildRoundedRectGeometryCacheEntry(IRoundedRectDrawable2D* shape);

        /// Rebuilds one cached text-geometry entry from the drawable's current authored state.
        void RebuildTextGeometryCacheEntry(TextGeometryCacheEntry& cacheEntry, ITextDrawable2D* text);

        /// Rebuilds one cached rounded-rectangle entry from the drawable's current authored state.
        void RebuildRoundedRectGeometryCacheEntry(RoundedRectGeometryCacheEntry& cacheEntry, IRoundedRectDrawable2D* shape);

        /// Appends rounded-corner triangle fan geometry to the supplied vertex list.
        void AppendRoundedCornerVertices(std::vector<Psp2DVertex>& vertices, const float3& position, const int2& size, int32_t roundedRadius, const byte4& color);

        /// Returns whether one text drawable should render through a pre-rendered static surface.
        bool ShouldUseStaticTextSurface(ITextDrawable2D* text) const;

        /// Rebuilds the pre-rendered static text surface for one cached entry.
        void RebuildStaticTextSurface(TextGeometryCacheEntry& cacheEntry, FontAsset* font);

        /// Releases one cached static text surface texture when the cache entry changes ownership.
        void ReleaseStaticTextSurface(TextGeometryCacheEntry& cacheEntry);

        /// Clears all cached 2D geometry so subsequent draws rebuild from current authored state.
        void ClearGeometryCaches();

        /// Draws one textured 2D triangle list using the PSP GU textured vertex path.
        void DrawTexturedTriangles(const Psp2DVertex* vertices, std::size_t vertexCount, RuntimeTexture* texture);

        /// Resolves the authored clip-region stack for one drawable into one effective screen-space clip rectangle.
        bool TryResolveClipRect(IDrawable2D* drawable, float4& clipRect);

        /// Applies one effective clip rectangle to the PSP GU scissor state for subsequent 2D draws.
        void ApplyClipRect(const float4& clipRect);

        /// Clears any active clip rectangle and restores unclipped PSP GU 2D drawing.
        void ClearClipRect();

        /// Converts one engine byte color into the ABGR8888 format expected by PSP GU.
        static std::uint32_t ConvertColorToAbgr(const byte4& color);

        /// Converts one normalized engine source rectangle into the PSP 2D texel-space coordinates expected by GU sprites.
        static float4 ConvertSourceRectToTexturePixels(const float4& sourceRect, RuntimeTexture* texture);

        /// Modulates one ABGR8888 source pixel by the supplied drawable tint.
        static std::uint32_t MultiplyAbgrColor(std::uint32_t sourcePixel, const byte4& tint);

        /// Blends one ABGR8888 source pixel over one destination pixel using standard source-alpha composition.
        static void BlendAbgrPixel(std::uint32_t sourcePixel, std::uint32_t& destinationPixel);

        /// Returns the reusable white runtime texture used to render solid 2D quads.
        PspRuntimeTexture* GetWhiteTexture();

    };
}
