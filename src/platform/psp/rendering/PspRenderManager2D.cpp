#include "platform/psp/rendering/PspRenderManager2D.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <pspgu.h>
#include <pspkernel.h>

#include "Entity.hpp"
#include "FontAsset.hpp"
#include "FontChar.hpp"
#include "IRenderQueue2D.hpp"
#include "IRoundedRectDrawable2D.hpp"
#include "ISpriteDrawable2D.hpp"
#include "ITextDrawable2D.hpp"
#include "RoundedRectComponent.hpp"
#include "ScrollComponent.hpp"
#include "TextComponent.hpp"
#include "TextLayoutUtils.hpp"
#include "ClipRectComponent.hpp"
#include "platform/psp/PspBootTrace.hpp"
#include "platform/psp/rendering/PspRuntimeTexture.hpp"

namespace helengine::psp::rendering {
    namespace {
        constexpr double Pi = 3.14159265358979323846;
        constexpr int RoundedCornerSegmentCount = 6;
        constexpr int PspScreenWidth = 480;
        constexpr int PspScreenHeight = 272;

        /// Binds one PSP runtime texture for GU sampling or disables texturing when no texture exists.
        void BindTexture(PspRuntimeTexture* texture) {
            if (texture == nullptr || !texture->HasPixels()) {
                sceGuDisable(GU_TEXTURE_2D);
                return;
            }

            sceKernelDcacheWritebackRange(
                const_cast<std::uint32_t*>(texture->GetPixelsAbgr8888()),
                static_cast<unsigned int>(texture->GetPixelCount() * sizeof(std::uint32_t)));

            sceGuEnable(GU_TEXTURE_2D);
            sceGuTexMode(GU_PSM_8888, 0, 0, 0);
            sceGuTexImage(0, texture->get_Width(), texture->get_Height(), texture->get_Width(), texture->GetPixelsAbgr8888());
            sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
            sceGuTexFilter(GU_NEAREST, GU_NEAREST);
            sceGuTexWrap(GU_REPEAT, GU_REPEAT);
        }

        /// Returns whether the drawable parent exists and participates in the active hierarchy.
        bool IsDrawableParentEnabled(Entity* parent) {
            return parent != nullptr && parent->get_IsHierarchyEnabled();
        }

        /// Returns whether one drawable matches the menu row orders currently under scroll-clip investigation.
        bool ShouldTraceMenuClip(IDrawable2D* drawable) {
            if (drawable == nullptr) {
                return false;
            }

            const uint8_t renderOrder = drawable->get_RenderOrder2D();
            return renderOrder == 33 || renderOrder == 34;
        }
    }

    /// Builds a PSP runtime texture from raw texture metadata and reuses cached instances by asset id.
    RuntimeTexture* PspRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        return TextureCache.BuildTextureFromRaw(data);
    }

    /// Renders one camera's queued 2D drawables in authored order.
    void PspRenderManager2D::RenderCamera(ICamera* camera) {
        if (camera == nullptr) {
            return;
        }

        IRenderQueue2D* renderQueue = camera->get_RenderQueue2D();
        if (renderQueue == nullptr || renderQueue->get_Count() <= 0) {
            return;
        }

        sceGuDisable(GU_LIGHT0);
        sceGuDisable(GU_LIGHTING);
        sceGuDisable(GU_DEPTH_TEST);
        sceGuDisable(GU_CULL_FACE);
        sceGuEnable(GU_BLEND);
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
        ClearClipRect();
        renderQueue->VisitOrdered(this);
        ClearClipRect();
    }

    /// Draws a rounded rectangle using one rectangular fill and border fallback.
    void PspRenderManager2D::DrawRoundedRect(IRoundedRectDrawable2D* shape) {
        if (shape == nullptr || !IsDrawableParentEnabled(shape->get_Parent())) {
            return;
        }

        const int2 size = shape->get_Size();
        if (size.X <= 0 || size.Y <= 0) {
            return;
        }

        const float3 position = shape->get_Parent()->get_Position();
        const int32_t borderThickness = std::max<int32_t>(0, static_cast<int32_t>(std::lround(shape->get_BorderThickness())));
        const float radius = std::max(0.0f, shape->get_Radius());

        if (borderThickness > 0) {
            DrawFilledRoundedRect(position, size, radius, shape->get_BorderColor());
        }

        const int32_t innerWidth = size.X - (borderThickness * 2);
        const int32_t innerHeight = size.Y - (borderThickness * 2);
        if (innerWidth <= 0 || innerHeight <= 0) {
            return;
        }

        DrawFilledRoundedRect(
            float3(position.X + borderThickness, position.Y + borderThickness, position.Z),
            int2(innerWidth, innerHeight),
            std::max(0.0f, radius - borderThickness),
            shape->get_FillColor());
    }

    /// Draws a textured sprite quad with the authored tint and source rectangle.
    void PspRenderManager2D::DrawSprite(ISpriteDrawable2D* sprite) {
        if (sprite == nullptr || !IsDrawableParentEnabled(sprite->get_Parent())) {
            return;
        } else if (sprite->get_Texture() == nullptr) {
            return;
        }

        RuntimeTexture* texture = sprite->get_Texture();
        int2 resolvedSize = sprite->get_Size();
        if (resolvedSize.X <= 0 || resolvedSize.Y <= 0) {
            resolvedSize = int2(texture->get_Width(), texture->get_Height());
        }

        DrawTexturedQuad(
            texture,
            sprite->get_Parent()->get_Position(),
            resolvedSize,
            sprite->get_SourceRect(),
            sprite->get_Color());
    }

    /// Draws bitmap-font text by emitting one glyph quad per visible character.
    void PspRenderManager2D::DrawText(ITextDrawable2D* text) {
        if (text == nullptr || !IsDrawableParentEnabled(text->get_Parent())) {
            return;
        }

        FontAsset* font = text->get_Font();
        if (font == nullptr || font->get_Texture() == nullptr || font->get_Characters() == nullptr || font->get_FontInfo() == nullptr) {
            return;
        }

        RuntimeTexture* atlasTexture = font->get_Texture();
        const float3 position = text->get_Parent()->get_Position();
        const byte4 color = text->get_Color();
        const double fontScale = std::max(static_cast<double>(text->get_FontScale()), 0.0001d);
        std::string content = text->get_Text();

        if (text->get_WrapText()) {
            const int2 textSize = text->get_Size();
            const int32_t wrapWidth = std::max<int32_t>(
                1,
                static_cast<int32_t>(std::lround(std::max<int32_t>(static_cast<int32_t>(textSize.X), 1) / fontScale)));
            content = TextLayoutUtils::WrapText(content, font, wrapWidth);
        }

        double offsetX = 0.0;
        double offsetY = 0.0;
        const double lineHeight = std::max(static_cast<double>(font->get_LineHeight()) * fontScale, 1.0d);
        const double baseX = std::round(position.X);
        const double baseY = std::round(position.Y);

        for (char character : content) {
            if (character == '\n') {
                offsetY += lineHeight;
                offsetX = 0.0;
                continue;
            }

            if (character == ' ') {
                offsetX += font->get_FontInfo()->get_SpaceWidth() * fontScale;
                continue;
            }

            FontChar glyph;
            if (!font->get_Characters()->TryGetValue(character, glyph)) {
                continue;
            }

            const double glyphWidth = glyph.SourceRect.Z * font->get_AtlasWidth() * fontScale;
            const double glyphHeight = glyph.SourceRect.W * font->get_AtlasHeight() * fontScale;
            const double snappedLineOffsetY = std::round(offsetY);
            const double drawX = std::round(baseX + offsetX);
            const double drawY = std::round(baseY + snappedLineOffsetY + (glyph.OffsetY * fontScale));
            const double drawRight = std::round(baseX + offsetX + glyphWidth);
            const double drawBottom = std::round(baseY + snappedLineOffsetY + (glyph.OffsetY * fontScale) + glyphHeight);

            DrawTexturedQuad(
                atlasTexture,
                float3(
                    static_cast<float>(drawX),
                    static_cast<float>(drawY),
                    position.Z),
                int2(
                    std::max<int32_t>(1, static_cast<int32_t>(drawRight - drawX)),
                    std::max<int32_t>(1, static_cast<int32_t>(drawBottom - drawY))),
                glyph.SourceRect,
                color);

            const double advanceWidth = glyph.AdvanceWidth > 0.0f
                ? glyph.AdvanceWidth * fontScale
                : glyphWidth;
            offsetX += advanceWidth;
        }
    }

    /// Visits one queued 2D drawable and dispatches it back through generated-core.
    void PspRenderManager2D::Visit(IDrawable2D* drawable) {
        if (drawable == nullptr) {
            return;
        }

        float4 clipRect;
        const int32_t menuRowTraceIndex = ResolveMenuRowTraceIndex(drawable);
        if (TryResolveClipRect(drawable, clipRect)) {
            if (menuRowTraceIndex >= 0 && !MenuRowTraceFlags[menuRowTraceIndex]) {
                Entity* parent = drawable->get_Parent();
                const float3 parentPosition = parent != nullptr ? parent->get_Position() : float3();
                psp::PspBootTrace::WriteLine(
                    std::string("MenuRowClip resolved index=") + std::to_string(menuRowTraceIndex)
                    + " y=" + std::to_string(parentPosition.Y)
                    + " rect=" + std::to_string(clipRect.X) + "," + std::to_string(clipRect.Y) + "," + std::to_string(clipRect.Z) + "," + std::to_string(clipRect.W));
                MenuRowTraceFlags[menuRowTraceIndex] = true;
            }
            ApplyClipRect(clipRect);
        } else {
            if (menuRowTraceIndex >= 0 && !MenuRowTraceFlags[menuRowTraceIndex]) {
                Entity* parent = drawable->get_Parent();
                const float3 parentPosition = parent != nullptr ? parent->get_Position() : float3();
                psp::PspBootTrace::WriteLine(
                    std::string("MenuRowClip none index=") + std::to_string(menuRowTraceIndex)
                    + " y=" + std::to_string(parentPosition.Y));
                MenuRowTraceFlags[menuRowTraceIndex] = true;
            }
            if (ShouldTraceMenuClip(drawable) && ClipDiagnosticCount < 512) {
                Entity* parent = drawable->get_Parent();
                const float3 parentPosition = parent != nullptr ? parent->get_Position() : float3();
                const char* drawableKind = "Drawable2D";
                if (dynamic_cast<IRoundedRectDrawable2D*>(drawable) != nullptr) {
                    drawableKind = "RoundedRect";
                } else if (dynamic_cast<ITextDrawable2D*>(drawable) != nullptr) {
                    drawableKind = "Text";
                } else if (dynamic_cast<ISpriteDrawable2D*>(drawable) != nullptr) {
                    drawableKind = "Sprite";
                }

                psp::PspBootTrace::WriteLine(
                    std::string("ClipRect none type=") + drawableKind
                    + " parentPos=" + std::to_string(parentPosition.X) + "," + std::to_string(parentPosition.Y)
                    + " order=" + std::to_string(drawable->get_RenderOrder2D()));
                ClipDiagnosticCount++;
            }
            ClearClipRect();
        }

        if (ShouldTraceMenuClip(drawable) && ClipDiagnosticCount < 1024) {
            Entity* parent = drawable->get_Parent();
            const float3 parentPosition = parent != nullptr ? parent->get_Position() : float3();
            const char* drawableKind = "Drawable2D";
            std::string sizeSummary = "size=na";

            if (IRoundedRectDrawable2D* roundedRect = dynamic_cast<IRoundedRectDrawable2D*>(drawable)) {
                drawableKind = "RoundedRect";
                const int2 size = roundedRect->get_Size();
                sizeSummary = "size=" + std::to_string(size.X) + "," + std::to_string(size.Y);
            } else if (ITextDrawable2D* text = dynamic_cast<ITextDrawable2D*>(drawable)) {
                drawableKind = "Text";
                const int2 size = text->get_Size();
                sizeSummary = "size=" + std::to_string(size.X) + "," + std::to_string(size.Y);
            } else if (ISpriteDrawable2D* sprite = dynamic_cast<ISpriteDrawable2D*>(drawable)) {
                drawableKind = "Sprite";
                const int2 size = sprite->get_Size();
                sizeSummary = "size=" + std::to_string(size.X) + "," + std::to_string(size.Y);
            }

            psp::PspBootTrace::WriteLine(
                std::string("DrawableState type=") + drawableKind
                + " parentPos=" + std::to_string(parentPosition.X) + "," + std::to_string(parentPosition.Y)
                + " order=" + std::to_string(drawable->get_RenderOrder2D())
                + " " + sizeSummary);
            ClipDiagnosticCount++;
        }

        drawable->Draw();
    }

    /// Draws one solid-colored screen-space quad.
    void PspRenderManager2D::DrawSolidQuad(const float3& position, const int2& size, const byte4& color) {
        if (size.X <= 0 || size.Y <= 0) {
            return;
        }

        DrawTexturedQuad(
            GetWhiteTexture(),
            position,
            size,
            float4(0.0f, 0.0f, 1.0f, 1.0f),
            color);
    }

    /// Draws one filled rounded rectangle using textured white geometry and triangle fans for the corners.
    void PspRenderManager2D::DrawFilledRoundedRect(const float3& position, const int2& size, float radius, const byte4& color) {
        if (size.X <= 0 || size.Y <= 0) {
            return;
        }

        const float clampedRadius = std::min(
            std::max(radius, 0.0f),
            std::min(size.X, size.Y) * 0.5f);
        if (clampedRadius <= 0.0f) {
            DrawSolidQuad(position, size, color);
            return;
        }

        const int32_t roundedRadius = std::max<int32_t>(1, static_cast<int32_t>(std::lround(clampedRadius)));

        DrawSolidQuad(
            float3(position.X + roundedRadius, position.Y, position.Z),
            int2(std::max<int32_t>(1, static_cast<int32_t>(size.X - (roundedRadius * 2))), size.Y),
            color);
        DrawSolidQuad(
            float3(position.X, position.Y + roundedRadius, position.Z),
            int2(roundedRadius, std::max<int32_t>(1, static_cast<int32_t>(size.Y - (roundedRadius * 2)))),
            color);
        DrawSolidQuad(
            float3(position.X + size.X - roundedRadius, position.Y + roundedRadius, position.Z),
            int2(roundedRadius, std::max<int32_t>(1, static_cast<int32_t>(size.Y - (roundedRadius * 2)))),
            color);

        std::vector<Psp2DVertex> vertices;
        vertices.reserve(static_cast<std::size_t>(RoundedCornerSegmentCount * 3 * 4));
        const std::uint32_t packedColor = ConvertColorToAbgr(color);

        for (int segmentIndex = 0; segmentIndex < RoundedCornerSegmentCount; segmentIndex++) {
            const double angle0 = Pi + ((Pi * 0.5 * segmentIndex) / RoundedCornerSegmentCount);
            const double angle1 = Pi + ((Pi * 0.5 * (segmentIndex + 1)) / RoundedCornerSegmentCount);

            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, position.X + roundedRadius, position.Y + roundedRadius, position.Z });
            vertices.push_back(Psp2DVertex {
                0.0f,
                0.0f,
                packedColor,
                static_cast<float>((position.X + roundedRadius) + (std::cos(angle0) * roundedRadius)),
                static_cast<float>((position.Y + roundedRadius) + (std::sin(angle0) * roundedRadius)),
                position.Z
            });
            vertices.push_back(Psp2DVertex {
                0.0f,
                0.0f,
                packedColor,
                static_cast<float>((position.X + roundedRadius) + (std::cos(angle1) * roundedRadius)),
                static_cast<float>((position.Y + roundedRadius) + (std::sin(angle1) * roundedRadius)),
                position.Z
            });
        }

        for (int segmentIndex = 0; segmentIndex < RoundedCornerSegmentCount; segmentIndex++) {
            const double angle0 = (Pi * 1.5) + ((Pi * 0.5 * segmentIndex) / RoundedCornerSegmentCount);
            const double angle1 = (Pi * 1.5) + ((Pi * 0.5 * (segmentIndex + 1)) / RoundedCornerSegmentCount);

            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, position.X + size.X - roundedRadius, position.Y + roundedRadius, position.Z });
            vertices.push_back(Psp2DVertex {
                0.0f,
                0.0f,
                packedColor,
                static_cast<float>((position.X + size.X - roundedRadius) + (std::cos(angle0) * roundedRadius)),
                static_cast<float>((position.Y + roundedRadius) + (std::sin(angle0) * roundedRadius)),
                position.Z
            });
            vertices.push_back(Psp2DVertex {
                0.0f,
                0.0f,
                packedColor,
                static_cast<float>((position.X + size.X - roundedRadius) + (std::cos(angle1) * roundedRadius)),
                static_cast<float>((position.Y + roundedRadius) + (std::sin(angle1) * roundedRadius)),
                position.Z
            });
        }

        for (int segmentIndex = 0; segmentIndex < RoundedCornerSegmentCount; segmentIndex++) {
            const double angle0 = (Pi * 0.5 * segmentIndex) / RoundedCornerSegmentCount;
            const double angle1 = (Pi * 0.5 * (segmentIndex + 1)) / RoundedCornerSegmentCount;

            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, position.X + size.X - roundedRadius, position.Y + size.Y - roundedRadius, position.Z });
            vertices.push_back(Psp2DVertex {
                0.0f,
                0.0f,
                packedColor,
                static_cast<float>((position.X + size.X - roundedRadius) + (std::cos(angle0) * roundedRadius)),
                static_cast<float>((position.Y + size.Y - roundedRadius) + (std::sin(angle0) * roundedRadius)),
                position.Z
            });
            vertices.push_back(Psp2DVertex {
                0.0f,
                0.0f,
                packedColor,
                static_cast<float>((position.X + size.X - roundedRadius) + (std::cos(angle1) * roundedRadius)),
                static_cast<float>((position.Y + size.Y - roundedRadius) + (std::sin(angle1) * roundedRadius)),
                position.Z
            });
        }

        for (int segmentIndex = 0; segmentIndex < RoundedCornerSegmentCount; segmentIndex++) {
            const double angle0 = (Pi * 0.5) + ((Pi * 0.5 * segmentIndex) / RoundedCornerSegmentCount);
            const double angle1 = (Pi * 0.5) + ((Pi * 0.5 * (segmentIndex + 1)) / RoundedCornerSegmentCount);

            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, position.X + roundedRadius, position.Y + size.Y - roundedRadius, position.Z });
            vertices.push_back(Psp2DVertex {
                0.0f,
                0.0f,
                packedColor,
                static_cast<float>((position.X + roundedRadius) + (std::cos(angle0) * roundedRadius)),
                static_cast<float>((position.Y + size.Y - roundedRadius) + (std::sin(angle0) * roundedRadius)),
                position.Z
            });
            vertices.push_back(Psp2DVertex {
                0.0f,
                0.0f,
                packedColor,
                static_cast<float>((position.X + roundedRadius) + (std::cos(angle1) * roundedRadius)),
                static_cast<float>((position.Y + size.Y - roundedRadius) + (std::sin(angle1) * roundedRadius)),
                position.Z
            });
        }
        DrawTexturedTriangles(vertices.data(), vertices.size(), GetWhiteTexture());
    }

    /// Draws one textured screen-space quad using PSP 2D texel-space UV coordinates.
    void PspRenderManager2D::DrawTexturedQuad(RuntimeTexture* texture, const float3& position, const int2& size, const float4& sourceRect, const byte4& color) {
        if (texture == nullptr || size.X <= 0 || size.Y <= 0) {
            return;
        }

        PspRuntimeTexture* pspTexture = dynamic_cast<PspRuntimeTexture*>(texture);
        if (pspTexture == nullptr) {
            throw std::runtime_error("PSP 2D textures must be PSP runtime textures.");
        }

        BindTexture(pspTexture);
        sceGuDisable(GU_DEPTH_TEST);

        Psp2DVertex* vertices = static_cast<Psp2DVertex*>(sceGuGetMemory(sizeof(Psp2DVertex) * 2u));
        const std::uint32_t packedColor = ConvertColorToAbgr(color);
        const float4 textureSourceRect = ConvertSourceRectToTexturePixels(sourceRect, texture);

        vertices[0].U = textureSourceRect.X;
        vertices[0].V = textureSourceRect.Y;
        vertices[0].Color = packedColor;
        vertices[0].X = position.X;
        vertices[0].Y = position.Y;
        vertices[0].Z = position.Z;

        vertices[1].U = textureSourceRect.X + textureSourceRect.Z;
        vertices[1].V = textureSourceRect.Y + textureSourceRect.W;
        vertices[1].Color = packedColor;
        vertices[1].X = position.X + size.X;
        vertices[1].Y = position.Y + size.Y;
        vertices[1].Z = position.Z;

        sceGuDrawArray(
            GU_SPRITES,
            GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
            2,
            nullptr,
            vertices);
    }

    /// Draws one textured 2D triangle list using the PSP GU textured vertex path.
    void PspRenderManager2D::DrawTexturedTriangles(const Psp2DVertex* vertices, std::size_t vertexCount, RuntimeTexture* texture) {
        if (vertices == nullptr || vertexCount < 3 || texture == nullptr) {
            return;
        }

        PspRuntimeTexture* pspTexture = dynamic_cast<PspRuntimeTexture*>(texture);
        if (pspTexture == nullptr) {
            throw std::runtime_error("PSP 2D triangle draws must use PSP runtime textures.");
        }

        BindTexture(pspTexture);
        sceGuDisable(GU_DEPTH_TEST);
        Psp2DVertex* drawVertices = static_cast<Psp2DVertex*>(sceGuGetMemory(sizeof(Psp2DVertex) * vertexCount));
        std::copy(vertices, vertices + vertexCount, drawVertices);

        sceGuDrawArray(
            GU_TRIANGLES,
            GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
            static_cast<int>(vertexCount),
            nullptr,
            drawVertices);
    }

    /// Resolves one drawable's nested clip regions into one effective rectangle in screen coordinates.
    bool PspRenderManager2D::TryResolveClipRect(IDrawable2D* drawable, float4& clipRect) {
        ClipRegionStackBuilder.BuildClipChain(drawable, &ClipChain);
        if (ClipChain.Count() <= 0) {
            return false;
        }

        clipRect = ClipChain[0]->GetClipRect();
        for (int32_t clipIndex = 1; clipIndex < ClipChain.Count(); clipIndex++) {
            clipRect = ClipRegionStackBuilder.Intersect(clipRect, ClipChain[clipIndex]->GetClipRect());
        }

        if (ShouldTraceMenuClip(drawable) && ClipDiagnosticCount < 512) {
            Entity* parent = drawable->get_Parent();
            const float3 parentPosition = parent != nullptr ? parent->get_Position() : float3();
            const char* drawableKind = "Drawable2D";
            if (dynamic_cast<IRoundedRectDrawable2D*>(drawable) != nullptr) {
                drawableKind = "RoundedRect";
            } else if (dynamic_cast<ITextDrawable2D*>(drawable) != nullptr) {
                drawableKind = "Text";
            } else if (dynamic_cast<ISpriteDrawable2D*>(drawable) != nullptr) {
                drawableKind = "Sprite";
            }

            std::string chainTypes;
            for (int32_t clipIndex = 0; clipIndex < ClipChain.Count(); clipIndex++) {
                if (clipIndex > 0) {
                    chainTypes += ",";
                }

                if (dynamic_cast<ScrollComponent*>(ClipChain[clipIndex]) != nullptr) {
                    chainTypes += "Scroll";
                } else if (dynamic_cast<ClipRectComponent*>(ClipChain[clipIndex]) != nullptr) {
                    chainTypes += "ClipRect";
                } else {
                    chainTypes += "Other";
                }
            }

            psp::PspBootTrace::WriteLine(
                std::string("ClipRect count=") + std::to_string(ClipChain.Count())
                + " types=" + chainTypes
                + " type=" + drawableKind
                + " parentPos=" + std::to_string(parentPosition.X) + "," + std::to_string(parentPosition.Y)
                + " order=" + std::to_string(drawable->get_RenderOrder2D())
                + " rect=" + std::to_string(clipRect.X) + "," + std::to_string(clipRect.Y) + "," + std::to_string(clipRect.Z) + "," + std::to_string(clipRect.W));
            ClipDiagnosticCount++;
        }

        return true;
    }

    /// Returns the trace slot for one menu-row drawable position or `-1` when the drawable is outside the current investigation scope.
    int32_t PspRenderManager2D::ResolveMenuRowTraceIndex(IDrawable2D* drawable) const {
        if (!ShouldTraceMenuClip(drawable)) {
            return -1;
        }

        Entity* parent = drawable->get_Parent();
        if (parent == nullptr) {
            return -1;
        }

        const float y = parent->get_Position().Y;
        static constexpr float RowStarts[] = {
            82.75f,
            106.0f,
            129.25f,
            152.5f,
            175.75f,
            199.0f,
            222.25f,
            245.5f,
            268.75f
        };

        for (int32_t rowIndex = 0; rowIndex < static_cast<int32_t>(std::size(RowStarts)); rowIndex++) {
            if (std::fabs(y - RowStarts[rowIndex]) <= 0.51f) {
                return rowIndex;
            }

            if (std::fabs((y - 4.5f) - RowStarts[rowIndex]) <= 0.51f) {
                return rowIndex;
            }
        }

        return -1;
    }

    /// Applies one drawable clip rectangle through the PSP GU scissor test.
    void PspRenderManager2D::ApplyClipRect(const float4& clipRect) {
        if (HasActiveClipRect
            && ActiveClipRect.X == clipRect.X
            && ActiveClipRect.Y == clipRect.Y
            && ActiveClipRect.Z == clipRect.Z
            && ActiveClipRect.W == clipRect.W) {
            return;
        }

        const int left = std::clamp(static_cast<int>(std::floor(clipRect.X)), 0, PspScreenWidth);
        const int top = std::clamp(static_cast<int>(std::floor(clipRect.Y)), 0, PspScreenHeight);
        const int right = std::clamp(static_cast<int>(std::ceil(clipRect.X + clipRect.Z)), left, PspScreenWidth);
        const int bottom = std::clamp(static_cast<int>(std::ceil(clipRect.Y + clipRect.W)), top, PspScreenHeight);

        sceGuEnable(GU_SCISSOR_TEST);
        sceGuScissor(left, top, right, bottom);

        HasActiveClipRect = true;
        ActiveClipRect = clipRect;
    }

    /// Clears the drawable clip rectangle so subsequent 2D draws are unrestricted.
    void PspRenderManager2D::ClearClipRect() {
        if (!HasActiveClipRect) {
            sceGuDisable(GU_SCISSOR_TEST);
            return;
        }

        sceGuDisable(GU_SCISSOR_TEST);
        HasActiveClipRect = false;
        ActiveClipRect = float4();
    }

    /// Converts one engine byte color into the ABGR8888 format expected by PSP GU.
    std::uint32_t PspRenderManager2D::ConvertColorToAbgr(const byte4& color) {
        return (static_cast<std::uint32_t>(color.W) << 24)
            | (static_cast<std::uint32_t>(color.Z) << 16)
            | (static_cast<std::uint32_t>(color.Y) << 8)
            | static_cast<std::uint32_t>(color.X);
    }

    /// Converts one normalized engine source rectangle into the PSP 2D texel-space coordinates expected by GU sprites.
    float4 PspRenderManager2D::ConvertSourceRectToTexturePixels(const float4& sourceRect, RuntimeTexture* texture) {
        if (texture == nullptr) {
            throw std::invalid_argument("PSP 2D source-rect conversion requires a runtime texture.");
        }

        return float4(
            sourceRect.X * texture->get_Width(),
            sourceRect.Y * texture->get_Height(),
            sourceRect.Z * texture->get_Width(),
            sourceRect.W * texture->get_Height());
    }

    /// Returns the reusable white runtime texture used to render solid 2D quads.
    PspRuntimeTexture* PspRenderManager2D::GetWhiteTexture() {
        if (WhiteTexture != nullptr) {
            return WhiteTexture;
        }

        WhiteTexture = new PspRuntimeTexture();
        WhiteTexture->set_Id("engine:psp:white-2d");
        WhiteTexture->set_Width(1);
        WhiteTexture->set_Height(1);

        std::vector<std::uint32_t> pixels;
        pixels.push_back(0xffffffffu);
        WhiteTexture->SetPixelsAbgr8888(std::move(pixels));
        return WhiteTexture;
    }
}
