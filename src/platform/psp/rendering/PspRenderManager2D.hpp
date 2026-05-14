#pragma once

#include <cstdint>

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
#include "platform/psp/rendering/PspRuntimeTexture.hpp"
#include "platform/psp/rendering/PspTextureCache.hpp"

namespace helengine::psp::rendering {
    /// Provides the PSP 2D runtime surface used by menus and other simple UI primitives.
    class PspRenderManager2D final : public RenderManager2D, public IRenderVisitor2D {
    public:
        /// Builds a PSP runtime texture from raw texture metadata.
        RuntimeTexture* BuildTextureFromRaw(TextureAsset* data) override;

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

        /// Stores cached PSP runtime textures built from cooked texture assets.
        PspTextureCache TextureCache;

        /// Builds the active clip chain for one drawable using the generated-core clip-region contract.
        ClipRegionStackBuilder2D ClipRegionStackBuilder;

        /// Stores the reusable clip-chain list used while resolving one drawable's effective clip rectangle.
        List<IClipRegion2D*> ClipChain;

        /// Stores the reusable white runtime texture used to render solid 2D quads through the proven textured path.
        PspRuntimeTexture* WhiteTexture = nullptr;

        /// Stores whether one drawable-scoped clip rectangle is currently active on the PSP GU.
        bool HasActiveClipRect = false;

        /// Stores the current drawable-scoped clip rectangle so redundant GU state changes can be skipped.
        float4 ActiveClipRect = float4();

        /// Draws one solid-colored screen-space quad.
        void DrawSolidQuad(const float3& position, const int2& size, const byte4& color);

        /// Draws one textured screen-space quad using normalized UV coordinates.
        void DrawTexturedQuad(RuntimeTexture* texture, const float3& position, const int2& size, const float4& sourceRect, const byte4& color);

        /// Draws one filled rounded rectangle using textured white geometry and triangle fans for the corners.
        void DrawFilledRoundedRect(const float3& position, const int2& size, float radius, const byte4& color);

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

        /// Returns the reusable white runtime texture used to render solid 2D quads.
        PspRuntimeTexture* GetWhiteTexture();
    };
}
