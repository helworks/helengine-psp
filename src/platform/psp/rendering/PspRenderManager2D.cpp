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
#include "FontInfo.hpp"
#include "IRenderQueue2D.hpp"
#include "IRoundedRectDrawable2D.hpp"
#include "ISpriteDrawable2D.hpp"
#include "ITextDrawable2D.hpp"
#include "RoundedRectComponent.hpp"
#include "TextComponent.hpp"
#include "TextLayoutAlignmentUtils.hpp"
#include "TextLayoutUtils.hpp"
#include "TextureAsset.hpp"
#include "platform/psp/rendering/PspRuntimeTexture.hpp"
#include "runtime/array.hpp"

namespace helengine::psp::rendering {
    namespace {
        constexpr double Pi = 3.14159265358979323846;
        constexpr int RoundedCornerSegmentCount = 6;
        constexpr int PspScreenWidth = 480;
        constexpr int PspScreenHeight = 272;

        /// Returns whether the drawable parent exists and participates in the active hierarchy.
        bool IsDrawableParentEnabled(Entity* parent) {
            return parent != nullptr && parent->get_IsHierarchyEnabled();
        }

    }

    /// Builds a PSP runtime texture from raw texture metadata and reuses cached instances by asset id.
    RuntimeTexture* PspRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        return TextureCache.BuildTextureFromRaw(data);
    }

    /// Releases one PSP runtime texture previously created by this renderer.
    void PspRenderManager2D::ReleaseTexture(RuntimeTexture* texture) {
        if (texture == nullptr) {
            throw std::invalid_argument("PSP 2D runtime texture release requires one texture instance.");
        }

        PspRuntimeTexture* pspTexture = dynamic_cast<PspRuntimeTexture*>(texture);
        if (pspTexture == nullptr) {
            throw std::runtime_error("PSP 2D texture release requires PSP runtime textures.");
        }
        if (pspTexture == WhiteTexture) {
            return;
        }

        ClearGeometryCaches();
        TextureCache.ReleaseTexture(pspTexture);
        pspTexture->Dispose();
        delete pspTexture;
    }

    /// Releases one PSP font asset and its native-owned object graph.
    void PspRenderManager2D::ReleaseFont(FontAsset* font) {
        if (font == nullptr) {
            throw std::invalid_argument("PSP 2D font release requires one font instance.");
        }

        ClearGeometryCaches();

        RuntimeTexture* texture = font->get_Texture();
        if (texture != nullptr && !texture->get_IsDisposed()) {
            ReleaseTexture(texture);
        }

        font->Dispose();
        delete font;
    }

    /// Flushes deferred PSP runtime texture deletions once the renderer reaches a safe boundary before the next scene begins loading.
    void PspRenderManager2D::FlushReleasedTextures() {
        TextureCache.FlushReleasedTextures();
    }

    /// Binds one PSP runtime texture for GU sampling or disables texturing when no texture exists.
    void PspRenderManager2D::BindTexture(PspRuntimeTexture* texture) {
        const std::uint64_t bindStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        std::uint64_t flushMicroseconds = 0;
        std::size_t byteCount = 0;
        if (texture == nullptr || !texture->HasPixels()) {
            BoundTexture = nullptr;
            sceGuDisable(GU_TEXTURE_2D);
            PspRenderProfiler::Record2DTextureBind(
                texture,
                byteCount,
                PspRenderProfiler::GetTimestampMicroseconds() - bindStartMicroseconds,
                flushMicroseconds);
            return;
        } else if (BoundTexture == texture) {
            PspRenderProfiler::Record2DTextureBind(
                texture,
                byteCount,
                PspRenderProfiler::GetTimestampMicroseconds() - bindStartMicroseconds,
                flushMicroseconds);
            return;
        }

        byteCount = texture->GetPixelCount() * sizeof(std::uint32_t);
        const std::uint64_t flushStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        sceKernelDcacheWritebackRange(
            const_cast<std::uint32_t*>(texture->GetPixelsAbgr8888()),
            static_cast<unsigned int>(byteCount));
        flushMicroseconds = PspRenderProfiler::GetTimestampMicroseconds() - flushStartMicroseconds;

        sceGuEnable(GU_TEXTURE_2D);
        sceGuTexMode(GU_PSM_8888, 0, 0, 0);
        sceGuTexImage(0, texture->get_Width(), texture->get_Height(), texture->GetTextureBufferWidth(), texture->GetPixelsAbgr8888());
        sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
        sceGuTexWrap(GU_REPEAT, GU_REPEAT);
        BoundTexture = texture;
        PspRenderProfiler::Record2DTextureBind(
            texture,
            byteCount,
            PspRenderProfiler::GetTimestampMicroseconds() - bindStartMicroseconds,
            flushMicroseconds);
    }

    /// Clears any cached bound-texture state at camera-pass boundaries.
    void PspRenderManager2D::ResetBoundTextureState() {
        BoundTexture = nullptr;
    }

    /// Renders one camera's queued 2D drawables in authored order.
    void PspRenderManager2D::RenderCamera(ICamera* camera) {
        const std::uint64_t renderStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        FlushReleasedTextures();
        FlushPendingWhiteTriangles();
        ResetBoundTextureState();
        if (camera == nullptr) {
            return;
        }

        IRenderQueue2D* renderQueue = camera->get_RenderQueue2D();
        if (renderQueue == nullptr || renderQueue->get_Count() <= 0) {
            return;
        }
        const int32_t drawableCount = renderQueue->get_Count();
        sceGuDisable(GU_LIGHT0);
        sceGuDisable(GU_LIGHTING);
        sceGuDisable(GU_DEPTH_TEST);
        sceGuDisable(GU_CULL_FACE);
        sceGuEnable(GU_BLEND);
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
        ClearClipRect();
        renderQueue->VisitOrdered(this);
        FlushPendingWhiteTriangles();
        ClearClipRect();
        ResetBoundTextureState();
        PspRenderProfiler::Record2DCamera(
            drawableCount,
            PspRenderProfiler::GetTimestampMicroseconds() - renderStartMicroseconds);
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
        const RoundedRectGeometryCacheEntry& cacheEntry = GetOrBuildRoundedRectGeometryCacheEntry(shape);
        DrawCachedRoundedRectGeometry(
            cacheEntry,
            shape->get_Parent()->get_Position(),
            shape->get_FillColor(),
            shape->get_BorderColor());
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

        Entity* parent = sprite->get_Parent();
        float3 scale = parent->get_Scale();
        const float width = static_cast<float>(static_cast<double>(resolvedSize.X) * static_cast<double>(scale.X));
        const float height = static_cast<float>(static_cast<double>(resolvedSize.Y) * static_cast<double>(scale.Y));
        float3 rotatedRight = float4::RotateVector(float3::get_UnitX(), parent->get_Orientation());
        const float rotationRadians = static_cast<float>(std::atan2(
            static_cast<double>(rotatedRight.Y),
            static_cast<double>(rotatedRight.X)));

        DrawTexturedQuadTransformed(
            texture,
            parent->get_Position(),
            width,
            height,
            sprite->get_SourceRect(),
            sprite->get_Color(),
            rotationRadians);
    }

    /// Draws bitmap-font text by batching all visible glyphs into one textured triangle submission.
    void PspRenderManager2D::DrawText(ITextDrawable2D* text) {
        const std::uint64_t drawStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        if (text == nullptr || !IsDrawableParentEnabled(text->get_Parent())) {
            return;
        }

        FontAsset* font = text->get_Font();
        if (font == nullptr || font->get_Texture() == nullptr || font->get_Characters() == nullptr || font->get_FontInfo() == nullptr) {
            return;
        }

        TextGeometryCacheEntry& cacheEntry = GetOrBuildTextGeometryCacheEntry(text);
        if (cacheEntry.UsesStaticSurface && cacheEntry.StaticSurfaceTexture != nullptr) {
            DrawTexturedQuad(
                cacheEntry.StaticSurfaceTexture,
                text->get_Parent()->get_Position(),
                cacheEntry.StaticSurfaceSize,
                cacheEntry.StaticSurfaceSourceRect,
                byte4(255, 255, 255, 255));
        } else if (!cacheEntry.Vertices.empty()) {
            DrawTexturedTrianglesTranslated(
                cacheEntry.Vertices.data(),
                cacheEntry.Vertices.size(),
                cacheEntry.Texture,
                text->get_Parent()->get_Position());
        }

        PspRenderProfiler::Record2DText(
            cacheEntry.GlyphCount,
            static_cast<int32_t>(cacheEntry.Content.size()),
            PspRenderProfiler::GetTimestampMicroseconds() - drawStartMicroseconds);
    }

    /// Visits one queued 2D drawable and dispatches it back through generated-core.
    void PspRenderManager2D::Visit(IDrawable2D* drawable) {
        if (drawable == nullptr) {
            return;
        }

        float4 clipRect;
        if (TryResolveClipRect(drawable, clipRect)) {
            ApplyClipRect(clipRect);
        } else {
            ClearClipRect();
        }

        drawable->Draw();
    }

    /// Draws one solid-colored screen-space quad.
    void PspRenderManager2D::DrawSolidQuad(const float3& position, const int2& size, const byte4& color) {
        if (size.X <= 0 || size.Y <= 0) {
            return;
        }

        AppendSolidQuadToWhiteBatch(position, size, color);
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
        AppendWhiteTrianglesToBatch(vertices);
    }

    /// Draws one cached rounded rectangle using geometry that only rebuilds when the authored inputs change.
    void PspRenderManager2D::DrawCachedRoundedRectGeometry(const RoundedRectGeometryCacheEntry& entry, const float3& positionOffset, const byte4& fillColor, const byte4& borderColor) {
        if (entry.HasBorder) {
            if (entry.BorderUsesSolidQuad) {
                AppendSolidQuadToWhiteBatch(
                    float3(
                        entry.BorderPosition.X + positionOffset.X,
                        entry.BorderPosition.Y + positionOffset.Y,
                        entry.BorderPosition.Z + positionOffset.Z),
                    entry.BorderSize,
                    borderColor);
            } else if (!entry.BorderVertices.empty()) {
                std::vector<Psp2DVertex> translatedVertices = entry.BorderVertices;
                ApplyColorToVertices(translatedVertices, borderColor);
                AppendWhiteTrianglesToBatchTranslated(translatedVertices, positionOffset);
            }
        }

        if (entry.HasInnerFill) {
            if (entry.InnerFillUsesSolidQuad) {
                AppendSolidQuadToWhiteBatch(
                    float3(
                        entry.InnerFillPosition.X + positionOffset.X,
                        entry.InnerFillPosition.Y + positionOffset.Y,
                        entry.InnerFillPosition.Z + positionOffset.Z),
                    entry.InnerFillSize,
                    fillColor);
            } else if (!entry.InnerFillVertices.empty()) {
                std::vector<Psp2DVertex> translatedVertices = entry.InnerFillVertices;
                ApplyColorToVertices(translatedVertices, fillColor);
                AppendWhiteTrianglesToBatchTranslated(translatedVertices, positionOffset);
            }
        }
    }

    /// Appends one solid-colored quad to the pending white-texture batch.
    void PspRenderManager2D::AppendSolidQuadToWhiteBatch(const float3& position, const int2& size, const byte4& color) {
        AppendSolidQuadVertices(PendingWhiteTriangles, position, size, color);
    }

    /// Appends one solid-colored quad directly to one cached white-triangle vertex list.
    void PspRenderManager2D::AppendSolidQuadVertices(std::vector<Psp2DVertex>& vertices, const float3& position, const int2& size, const byte4& color) {
        const std::uint32_t packedColor = ConvertColorToAbgr(color);
        const float left = position.X;
        const float top = position.Y;
        const float right = position.X + size.X;
        const float bottom = position.Y + size.Y;
        const float z = position.Z;

        vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, left, top, z });
        vertices.push_back(Psp2DVertex { 1.0f, 0.0f, packedColor, right, top, z });
        vertices.push_back(Psp2DVertex { 0.0f, 1.0f, packedColor, left, bottom, z });
        vertices.push_back(Psp2DVertex { 0.0f, 1.0f, packedColor, left, bottom, z });
        vertices.push_back(Psp2DVertex { 1.0f, 0.0f, packedColor, right, top, z });
        vertices.push_back(Psp2DVertex { 1.0f, 1.0f, packedColor, right, bottom, z });
    }

    /// Appends one list of white-texture triangles to the pending batch.
    void PspRenderManager2D::AppendWhiteTrianglesToBatch(const std::vector<Psp2DVertex>& vertices) {
        if (vertices.empty()) {
            return;
        }

        PendingWhiteTriangles.insert(PendingWhiteTriangles.end(), vertices.begin(), vertices.end());
    }

    /// Appends one list of white-texture triangles to the pending batch after applying one world-space offset.
    void PspRenderManager2D::AppendWhiteTrianglesToBatchTranslated(const std::vector<Psp2DVertex>& vertices, const float3& positionOffset) {
        if (vertices.empty()) {
            return;
        }

        const std::size_t startIndex = PendingWhiteTriangles.size();
        PendingWhiteTriangles.insert(PendingWhiteTriangles.end(), vertices.begin(), vertices.end());
        for (std::size_t index = startIndex; index < PendingWhiteTriangles.size(); index++) {
            PendingWhiteTriangles[index].X += positionOffset.X;
            PendingWhiteTriangles[index].Y += positionOffset.Y;
            PendingWhiteTriangles[index].Z += positionOffset.Z;
        }
    }

    /// Flushes any pending white-texture batch before state changes or frame completion.
    void PspRenderManager2D::FlushPendingWhiteTriangles() {
        if (PendingWhiteTriangles.empty()) {
            return;
        }

        DrawTexturedTriangles(PendingWhiteTriangles.data(), PendingWhiteTriangles.size(), GetWhiteTexture());
        PendingWhiteTriangles.clear();
    }

    /// Finds or rebuilds the cached text geometry for one drawable.
    PspRenderManager2D::TextGeometryCacheEntry& PspRenderManager2D::GetOrBuildTextGeometryCacheEntry(ITextDrawable2D* text) {
        for (TextGeometryCacheEntry& entry : TextGeometryCacheEntries) {
            if (entry.Drawable != text) {
                continue;
            }

            FontAsset* font = text->get_Font();
            RuntimeTexture* texture = font != nullptr ? font->get_Texture() : nullptr;
            const int2 size = text->get_Size();
            const byte4 color = text->get_Color();
            const bool wrapText = text->get_WrapText();
            const TextAlignment alignment = text->get_Alignment();
            const float fontScale = text->get_FontScale();
            const std::string textValue = text->get_Text();
            const bool usesStaticSurface = ShouldUseStaticTextSurface(text);
            if (entry.Texture == texture
                && entry.Size.X == size.X
                && entry.Size.Y == size.Y
                && entry.Color.X == color.X
                && entry.Color.Y == color.Y
                && entry.Color.Z == color.Z
                && entry.Color.W == color.W
                && entry.WrapText == wrapText
                && entry.Alignment == alignment
                && entry.FontScale == fontScale
                && entry.UsesStaticSurface == usesStaticSurface
                && entry.RawText == textValue) {
                return entry;
            }

            RebuildTextGeometryCacheEntry(entry, text);
            return entry;
        }

        TextGeometryCacheEntries.push_back(TextGeometryCacheEntry());
        TextGeometryCacheEntries.back().Drawable = text;
        RebuildTextGeometryCacheEntry(TextGeometryCacheEntries.back(), text);
        return TextGeometryCacheEntries.back();
    }

    /// Finds or rebuilds the cached rounded-rectangle geometry for one drawable.
    PspRenderManager2D::RoundedRectGeometryCacheEntry& PspRenderManager2D::GetOrBuildRoundedRectGeometryCacheEntry(IRoundedRectDrawable2D* shape) {
        for (RoundedRectGeometryCacheEntry& entry : RoundedRectGeometryCacheEntries) {
            if (entry.Drawable != shape) {
                continue;
            }

            const int2 size = shape->get_Size();
            const float radius = shape->get_Radius();
            const float borderThickness = shape->get_BorderThickness();
            if (entry.Size.X == size.X
                && entry.Size.Y == size.Y
                && entry.Radius == radius
                && entry.BorderThickness == borderThickness) {
                return entry;
            }

            RebuildRoundedRectGeometryCacheEntry(entry, shape);
            return entry;
        }

        RoundedRectGeometryCacheEntries.push_back(RoundedRectGeometryCacheEntry());
        RoundedRectGeometryCacheEntries.back().Drawable = shape;
        RebuildRoundedRectGeometryCacheEntry(RoundedRectGeometryCacheEntries.back(), shape);
        return RoundedRectGeometryCacheEntries.back();
    }

    /// Rebuilds one cached text-geometry entry from the drawable's current authored state.
    void PspRenderManager2D::RebuildTextGeometryCacheEntry(TextGeometryCacheEntry& cacheEntry, ITextDrawable2D* text) {
        FontAsset* font = text->get_Font();
        RuntimeTexture* atlasTexture = font->get_Texture();
        const byte4 color = text->get_Color();
        const float fontScaleValue = text->get_FontScale();
        const double fontScale = std::max(static_cast<double>(fontScaleValue), 0.0001d);
        std::string content = text->get_Text();

        if (text->get_WrapText()) {
            const int2 textSize = text->get_Size();
            const int32_t wrapWidth = std::max<int32_t>(
                1,
                static_cast<int32_t>(std::lround(std::max<int32_t>(static_cast<int32_t>(textSize.X), 1) / fontScale)));
            content = TextLayoutUtils::WrapText(content, font, wrapWidth);
        }

        cacheEntry.Texture = atlasTexture;
        cacheEntry.Content = content;
        cacheEntry.RawText = text->get_Text();
        cacheEntry.Size = text->get_Size();
        cacheEntry.Color = color;
        cacheEntry.WrapText = text->get_WrapText();
        cacheEntry.Alignment = text->get_Alignment();
        cacheEntry.FontScale = fontScaleValue;
        cacheEntry.GlyphCount = 0;
        cacheEntry.UsesStaticSurface = ShouldUseStaticTextSurface(text);
        cacheEntry.Vertices.clear();
        cacheEntry.Vertices.reserve(static_cast<std::size_t>(content.size()) * 6u);
        if (!cacheEntry.UsesStaticSurface) {
            ReleaseStaticTextSurface(cacheEntry);
        }

        double offsetX = 0.0;
        double offsetY = 0.0;
        const double lineHeight = std::max(static_cast<double>(font->get_LineHeight()) * fontScale, 1.0d);
        const double baseX = 0.0;
        const double baseY = 0.0;
        const int32_t layoutWidth = text->get_Size().X;
        const std::uint32_t packedColor = ConvertColorToAbgr(color);
        std::vector<double> lineOffsets;
        lineOffsets.reserve(static_cast<std::size_t>(std::count(content.begin(), content.end(), '\n')) + 1u);
        std::string currentLine;
        currentLine.reserve(content.size());
        for (char character : content) {
            if (character == '\r') {
                continue;
            }

            if (character == '\n') {
                const double visibleLineWidth = TextLayoutAlignmentUtils::MeasureVisibleLineWidth(
                    currentLine,
                    font,
                    fontScale,
                    static_cast<double>(font->get_AtlasWidth()));
                lineOffsets.push_back(TextLayoutAlignmentUtils::ResolveHorizontalOffset(
                    text->get_Alignment(),
                    layoutWidth,
                    visibleLineWidth));
                currentLine.clear();
                continue;
            }

            currentLine.push_back(character);
        }

        const double trailingVisibleLineWidth = TextLayoutAlignmentUtils::MeasureVisibleLineWidth(
            currentLine,
            font,
            fontScale,
            static_cast<double>(font->get_AtlasWidth()));
        lineOffsets.push_back(TextLayoutAlignmentUtils::ResolveHorizontalOffset(
            text->get_Alignment(),
            layoutWidth,
            trailingVisibleLineWidth));
        std::size_t lineIndex = 0u;
        double lineOffsetX = lineOffsets.empty() ? 0.0 : lineOffsets[0];

        for (char character : content) {
            if (character == '\r') {
                continue;
            }

            if (character == '\n') {
                offsetY += lineHeight;
                offsetX = 0.0;
                lineIndex++;
                lineOffsetX = lineIndex < lineOffsets.size() ? lineOffsets[lineIndex] : 0.0;
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
            cacheEntry.GlyphCount++;

            const double glyphWidth = glyph.SourceRect.Z * font->get_AtlasWidth() * fontScale;
            const double glyphHeight = glyph.SourceRect.W * font->get_AtlasHeight() * fontScale;
            const double snappedLineOffsetY = std::round(offsetY);
            const double drawX = std::round(baseX + lineOffsetX + offsetX);
            const double drawY = std::round(baseY + snappedLineOffsetY + (glyph.OffsetY * fontScale));
            const double drawRight = std::round(baseX + lineOffsetX + offsetX + glyphWidth);
            const double drawBottom = std::round(baseY + snappedLineOffsetY + (glyph.OffsetY * fontScale) + glyphHeight);
            const float4 textureSourceRect = ConvertSourceRectToTexturePixels(glyph.SourceRect, atlasTexture);
            const float left = static_cast<float>(drawX);
            const float top = static_cast<float>(drawY);
            const float right = static_cast<float>(std::max(drawX + 1.0, drawRight));
            const float bottom = static_cast<float>(std::max(drawY + 1.0, drawBottom));
            const float z = 0.0f;
            const float sourceLeft = textureSourceRect.X;
            const float sourceTop = textureSourceRect.Y;
            const float sourceRight = textureSourceRect.X + textureSourceRect.Z;
            const float sourceBottom = textureSourceRect.Y + textureSourceRect.W;

            cacheEntry.Vertices.push_back(Psp2DVertex { sourceLeft, sourceTop, packedColor, left, top, z });
            cacheEntry.Vertices.push_back(Psp2DVertex { sourceRight, sourceTop, packedColor, right, top, z });
            cacheEntry.Vertices.push_back(Psp2DVertex { sourceLeft, sourceBottom, packedColor, left, bottom, z });
            cacheEntry.Vertices.push_back(Psp2DVertex { sourceLeft, sourceBottom, packedColor, left, bottom, z });
            cacheEntry.Vertices.push_back(Psp2DVertex { sourceRight, sourceTop, packedColor, right, top, z });
            cacheEntry.Vertices.push_back(Psp2DVertex { sourceRight, sourceBottom, packedColor, right, bottom, z });

            const double advanceWidth = glyph.AdvanceWidth > 0.0f
                ? glyph.AdvanceWidth * fontScale
                : glyphWidth;
            offsetX += advanceWidth;
        }

        if (cacheEntry.UsesStaticSurface) {
            RebuildStaticTextSurface(cacheEntry, font);
            cacheEntry.Vertices.clear();
        }
    }

    /// Rebuilds one cached rounded-rectangle entry from the drawable's current authored state.
    void PspRenderManager2D::RebuildRoundedRectGeometryCacheEntry(RoundedRectGeometryCacheEntry& cacheEntry, IRoundedRectDrawable2D* shape) {
        const int2 size = shape->get_Size();
        const int32_t borderThickness = std::max<int32_t>(0, static_cast<int32_t>(std::lround(shape->get_BorderThickness())));
        const float radius = std::max(0.0f, shape->get_Radius());
        const byte4 fillColor = shape->get_FillColor();
        const byte4 borderColor = shape->get_BorderColor();
        const int32_t innerWidth = size.X - (borderThickness * 2);
        const int32_t innerHeight = size.Y - (borderThickness * 2);

        cacheEntry.Size = size;
        cacheEntry.Radius = shape->get_Radius();
        cacheEntry.BorderThickness = shape->get_BorderThickness();
        cacheEntry.HasBorder = borderThickness > 0;
        cacheEntry.HasInnerFill = innerWidth > 0 && innerHeight > 0;
        cacheEntry.BorderPosition = float3(0.0f, 0.0f, 0.0f);
        cacheEntry.BorderSize = size;
        cacheEntry.BorderRadius = radius;
        cacheEntry.InnerFillPosition = float3(static_cast<float>(borderThickness), static_cast<float>(borderThickness), 0.0f);
        cacheEntry.InnerFillSize = int2(innerWidth, innerHeight);
        cacheEntry.InnerFillRadius = std::max(0.0f, radius - borderThickness);
        cacheEntry.BorderVertices.clear();
        cacheEntry.InnerFillVertices.clear();
        cacheEntry.BorderUsesSolidQuad = false;
        cacheEntry.InnerFillUsesSolidQuad = false;

        if (cacheEntry.HasBorder) {
            const float clampedBorderRadius = std::min(
                std::max(cacheEntry.BorderRadius, 0.0f),
                std::min(cacheEntry.BorderSize.X, cacheEntry.BorderSize.Y) * 0.5f);
            if (clampedBorderRadius <= 0.0f) {
                cacheEntry.BorderUsesSolidQuad = true;
            } else {
                const int32_t roundedRadius = std::max<int32_t>(1, static_cast<int32_t>(std::lround(clampedBorderRadius)));
                AppendSolidQuadVertices(
                    cacheEntry.BorderVertices,
                    float3(cacheEntry.BorderPosition.X + roundedRadius, cacheEntry.BorderPosition.Y, cacheEntry.BorderPosition.Z),
                    int2(std::max<int32_t>(1, cacheEntry.BorderSize.X - (roundedRadius * 2)), cacheEntry.BorderSize.Y),
                    borderColor);
                AppendSolidQuadVertices(
                    cacheEntry.BorderVertices,
                    float3(cacheEntry.BorderPosition.X, cacheEntry.BorderPosition.Y + roundedRadius, cacheEntry.BorderPosition.Z),
                    int2(roundedRadius, std::max<int32_t>(1, cacheEntry.BorderSize.Y - (roundedRadius * 2))),
                    borderColor);
                AppendSolidQuadVertices(
                    cacheEntry.BorderVertices,
                    float3(cacheEntry.BorderPosition.X + cacheEntry.BorderSize.X - roundedRadius, cacheEntry.BorderPosition.Y + roundedRadius, cacheEntry.BorderPosition.Z),
                    int2(roundedRadius, std::max<int32_t>(1, cacheEntry.BorderSize.Y - (roundedRadius * 2))),
                    borderColor);
                AppendRoundedCornerVertices(cacheEntry.BorderVertices, cacheEntry.BorderPosition, cacheEntry.BorderSize, roundedRadius, borderColor);
            }
        }

        if (cacheEntry.HasInnerFill) {
            const float clampedInnerRadius = std::min(
                std::max(cacheEntry.InnerFillRadius, 0.0f),
                std::min(cacheEntry.InnerFillSize.X, cacheEntry.InnerFillSize.Y) * 0.5f);
            if (clampedInnerRadius <= 0.0f) {
                cacheEntry.InnerFillUsesSolidQuad = true;
            } else {
                const int32_t roundedRadius = std::max<int32_t>(1, static_cast<int32_t>(std::lround(clampedInnerRadius)));
                AppendSolidQuadVertices(
                    cacheEntry.InnerFillVertices,
                    float3(cacheEntry.InnerFillPosition.X + roundedRadius, cacheEntry.InnerFillPosition.Y, cacheEntry.InnerFillPosition.Z),
                    int2(std::max<int32_t>(1, cacheEntry.InnerFillSize.X - (roundedRadius * 2)), cacheEntry.InnerFillSize.Y),
                    fillColor);
                AppendSolidQuadVertices(
                    cacheEntry.InnerFillVertices,
                    float3(cacheEntry.InnerFillPosition.X, cacheEntry.InnerFillPosition.Y + roundedRadius, cacheEntry.InnerFillPosition.Z),
                    int2(roundedRadius, std::max<int32_t>(1, cacheEntry.InnerFillSize.Y - (roundedRadius * 2))),
                    fillColor);
                AppendSolidQuadVertices(
                    cacheEntry.InnerFillVertices,
                    float3(cacheEntry.InnerFillPosition.X + cacheEntry.InnerFillSize.X - roundedRadius, cacheEntry.InnerFillPosition.Y + roundedRadius, cacheEntry.InnerFillPosition.Z),
                    int2(roundedRadius, std::max<int32_t>(1, cacheEntry.InnerFillSize.Y - (roundedRadius * 2))),
                    fillColor);
                AppendRoundedCornerVertices(cacheEntry.InnerFillVertices, cacheEntry.InnerFillPosition, cacheEntry.InnerFillSize, roundedRadius, fillColor);
            }
        }
    }

    /// Appends rounded-corner triangle fan geometry to the supplied vertex list.
    void PspRenderManager2D::AppendRoundedCornerVertices(std::vector<Psp2DVertex>& vertices, const float3& position, const int2& size, int32_t roundedRadius, const byte4& color) {
        vertices.reserve(static_cast<std::size_t>(RoundedCornerSegmentCount * 3 * 4));
        const std::uint32_t packedColor = ConvertColorToAbgr(color);

        for (int segmentIndex = 0; segmentIndex < RoundedCornerSegmentCount; segmentIndex++) {
            const double angle0 = Pi + ((Pi * 0.5 * segmentIndex) / RoundedCornerSegmentCount);
            const double angle1 = Pi + ((Pi * 0.5 * (segmentIndex + 1)) / RoundedCornerSegmentCount);

            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, position.X + roundedRadius, position.Y + roundedRadius, position.Z });
            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, static_cast<float>((position.X + roundedRadius) + (std::cos(angle0) * roundedRadius)), static_cast<float>((position.Y + roundedRadius) + (std::sin(angle0) * roundedRadius)), position.Z });
            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, static_cast<float>((position.X + roundedRadius) + (std::cos(angle1) * roundedRadius)), static_cast<float>((position.Y + roundedRadius) + (std::sin(angle1) * roundedRadius)), position.Z });
        }

        for (int segmentIndex = 0; segmentIndex < RoundedCornerSegmentCount; segmentIndex++) {
            const double angle0 = (Pi * 1.5) + ((Pi * 0.5 * segmentIndex) / RoundedCornerSegmentCount);
            const double angle1 = (Pi * 1.5) + ((Pi * 0.5 * (segmentIndex + 1)) / RoundedCornerSegmentCount);

            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, position.X + size.X - roundedRadius, position.Y + roundedRadius, position.Z });
            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, static_cast<float>((position.X + size.X - roundedRadius) + (std::cos(angle0) * roundedRadius)), static_cast<float>((position.Y + roundedRadius) + (std::sin(angle0) * roundedRadius)), position.Z });
            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, static_cast<float>((position.X + size.X - roundedRadius) + (std::cos(angle1) * roundedRadius)), static_cast<float>((position.Y + roundedRadius) + (std::sin(angle1) * roundedRadius)), position.Z });
        }

        for (int segmentIndex = 0; segmentIndex < RoundedCornerSegmentCount; segmentIndex++) {
            const double angle0 = (Pi * 0.5 * segmentIndex) / RoundedCornerSegmentCount;
            const double angle1 = (Pi * 0.5 * (segmentIndex + 1)) / RoundedCornerSegmentCount;

            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, position.X + size.X - roundedRadius, position.Y + size.Y - roundedRadius, position.Z });
            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, static_cast<float>((position.X + size.X - roundedRadius) + (std::cos(angle0) * roundedRadius)), static_cast<float>((position.Y + size.Y - roundedRadius) + (std::sin(angle0) * roundedRadius)), position.Z });
            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, static_cast<float>((position.X + size.X - roundedRadius) + (std::cos(angle1) * roundedRadius)), static_cast<float>((position.Y + size.Y - roundedRadius) + (std::sin(angle1) * roundedRadius)), position.Z });
        }

        for (int segmentIndex = 0; segmentIndex < RoundedCornerSegmentCount; segmentIndex++) {
            const double angle0 = (Pi * 0.5) + ((Pi * 0.5 * segmentIndex) / RoundedCornerSegmentCount);
            const double angle1 = (Pi * 0.5) + ((Pi * 0.5 * (segmentIndex + 1)) / RoundedCornerSegmentCount);

            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, position.X + roundedRadius, position.Y + size.Y - roundedRadius, position.Z });
            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, static_cast<float>((position.X + roundedRadius) + (std::cos(angle0) * roundedRadius)), static_cast<float>((position.Y + size.Y - roundedRadius) + (std::sin(angle0) * roundedRadius)), position.Z });
            vertices.push_back(Psp2DVertex { 0.0f, 0.0f, packedColor, static_cast<float>((position.X + roundedRadius) + (std::cos(angle1) * roundedRadius)), static_cast<float>((position.Y + size.Y - roundedRadius) + (std::sin(angle1) * roundedRadius)), position.Z });
        }
    }

    /// Overwrites the packed color on one cached triangle list without changing its geometry.
    void PspRenderManager2D::ApplyColorToVertices(std::vector<Psp2DVertex>& vertices, const byte4& color) {
        const std::uint32_t packedColor = ConvertColorToAbgr(color);
        for (Psp2DVertex& vertex : vertices) {
            vertex.Color = packedColor;
        }
    }

    /// Returns whether one text drawable should render through a pre-rendered static surface.
    bool PspRenderManager2D::ShouldUseStaticTextSurface(ITextDrawable2D* text) const {
        (void)text;
        return false;
    }

    /// Returns the next power-of-two texture dimension required by the PSP GU.
    int32_t PspRenderManager2D::GetNextPowerOfTwoDimension(int32_t value) {
        if (value <= 1) {
            return 1;
        }

        int32_t dimension = 1;
        while (dimension < value) {
            dimension <<= 1;
        }

        return dimension;
    }

    /// Rebuilds the pre-rendered static text surface for one cached entry.
    void PspRenderManager2D::RebuildStaticTextSurface(TextGeometryCacheEntry& cacheEntry, FontAsset* font) {
        if (font == nullptr || cacheEntry.Content.empty() || cacheEntry.GlyphCount <= 0 || cacheEntry.Vertices.empty()) {
            ReleaseStaticTextSurface(cacheEntry);
            return;
        }

        PspRuntimeTexture* atlasTexture = dynamic_cast<PspRuntimeTexture*>(font->get_Texture());
        if (atlasTexture == nullptr || !atlasTexture->HasPixels()) {
            ReleaseStaticTextSurface(cacheEntry);
            return;
        }

        float minimumX = std::numeric_limits<float>::max();
        float minimumY = std::numeric_limits<float>::max();
        float maximumX = std::numeric_limits<float>::lowest();
        float maximumY = std::numeric_limits<float>::lowest();
        for (const Psp2DVertex& vertex : cacheEntry.Vertices) {
            minimumX = std::min(minimumX, vertex.X);
            minimumY = std::min(minimumY, vertex.Y);
            maximumX = std::max(maximumX, vertex.X);
            maximumY = std::max(maximumY, vertex.Y);
        }

        if (minimumX >= maximumX || minimumY >= maximumY) {
            ReleaseStaticTextSurface(cacheEntry);
            return;
        }

        const int32_t visibleSurfaceWidth = std::max<int32_t>(1, static_cast<int32_t>(std::ceil(maximumX - minimumX)));
        const int32_t visibleSurfaceHeight = std::max<int32_t>(1, static_cast<int32_t>(std::ceil(maximumY - minimumY)));
        const int32_t textureWidth = GetNextPowerOfTwoDimension(visibleSurfaceWidth);
        const int32_t textureHeight = GetNextPowerOfTwoDimension(visibleSurfaceHeight);
        std::vector<std::uint32_t> surfacePixels(static_cast<std::size_t>(textureWidth) * static_cast<std::size_t>(textureHeight), 0u);

        const std::uint32_t* atlasPixels = atlasTexture->GetPixelsAbgr8888();
        const int32_t atlasWidth = atlasTexture->get_Width();
        const int32_t atlasHeight = atlasTexture->get_Height();
        const double fontScale = std::max(static_cast<double>(cacheEntry.FontScale), 0.0001d);
        const double lineHeight = std::max(static_cast<double>(font->get_LineHeight()) * fontScale, 1.0d);
        const double baseX = 0.0;
        const double baseY = 0.0;
        const int32_t layoutWidth = cacheEntry.Size.X;
        std::vector<double> lineOffsets;
        lineOffsets.reserve(static_cast<std::size_t>(std::count(cacheEntry.Content.begin(), cacheEntry.Content.end(), '\n')) + 1u);
        std::string currentLine;
        currentLine.reserve(cacheEntry.Content.size());
        for (char character : cacheEntry.Content) {
            if (character == '\r') {
                continue;
            }

            if (character == '\n') {
                const double visibleLineWidth = TextLayoutAlignmentUtils::MeasureVisibleLineWidth(
                    currentLine,
                    font,
                    fontScale,
                    static_cast<double>(font->get_AtlasWidth()));
                lineOffsets.push_back(TextLayoutAlignmentUtils::ResolveHorizontalOffset(
                    cacheEntry.Alignment,
                    layoutWidth,
                    visibleLineWidth));
                currentLine.clear();
                continue;
            }

            currentLine.push_back(character);
        }

        const double trailingVisibleLineWidth = TextLayoutAlignmentUtils::MeasureVisibleLineWidth(
            currentLine,
            font,
            fontScale,
            static_cast<double>(font->get_AtlasWidth()));
        lineOffsets.push_back(TextLayoutAlignmentUtils::ResolveHorizontalOffset(
            cacheEntry.Alignment,
            layoutWidth,
            trailingVisibleLineWidth));

        double offsetX = 0.0;
        double offsetY = 0.0;
        std::size_t lineIndex = 0u;
        double lineOffsetX = lineOffsets.empty() ? 0.0 : lineOffsets[0];
        for (char character : cacheEntry.Content) {
            if (character == '\r') {
                continue;
            }

            if (character == '\n') {
                offsetY += lineHeight;
                offsetX = 0.0;
                lineIndex++;
                lineOffsetX = lineIndex < lineOffsets.size() ? lineOffsets[lineIndex] : 0.0;
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
            const double drawX = std::round(baseX + lineOffsetX + offsetX);
            const double drawY = std::round(baseY + snappedLineOffsetY + (glyph.OffsetY * fontScale));
            const double drawRight = std::round(baseX + lineOffsetX + offsetX + glyphWidth);
            const double drawBottom = std::round(baseY + snappedLineOffsetY + (glyph.OffsetY * fontScale) + glyphHeight);
            const int32_t destinationLeft = std::max<int32_t>(0, static_cast<int32_t>(std::lround(drawX - minimumX)));
            const int32_t destinationTop = std::max<int32_t>(0, static_cast<int32_t>(std::lround(drawY - minimumY)));
            const int32_t destinationRight = std::min<int32_t>(visibleSurfaceWidth, static_cast<int32_t>(std::lround(std::max(drawX + 1.0, drawRight) - minimumX)));
            const int32_t destinationBottom = std::min<int32_t>(visibleSurfaceHeight, static_cast<int32_t>(std::lround(std::max(drawY + 1.0, drawBottom) - minimumY)));
            const int32_t destinationWidth = destinationRight - destinationLeft;
            const int32_t destinationHeight = destinationBottom - destinationTop;
            if (destinationWidth > 0 && destinationHeight > 0) {
                const float4 atlasSourceRect = ConvertSourceRectToTexturePixels(glyph.SourceRect, atlasTexture);
                const int32_t sourceLeft = static_cast<int32_t>(std::floor(static_cast<double>(atlasSourceRect.X)));
                const int32_t sourceTop = static_cast<int32_t>(std::floor(static_cast<double>(atlasSourceRect.Y)));
                const int32_t sourceWidth = std::max<int32_t>(1, static_cast<int32_t>(std::round(atlasSourceRect.Z)));
                const int32_t sourceHeight = std::max<int32_t>(1, static_cast<int32_t>(std::round(atlasSourceRect.W)));

                for (int32_t destinationY = 0; destinationY < destinationHeight; destinationY++) {
                    const int32_t targetY = destinationTop + destinationY;
                    const int32_t sourceSampleOffsetY = (destinationY * sourceHeight) / destinationHeight;
                    const int32_t sampleY = std::clamp<int32_t>(
                        sourceTop + sourceSampleOffsetY,
                        0,
                        static_cast<int32_t>(atlasHeight - 1));
                    for (int32_t destinationX = 0; destinationX < destinationWidth; destinationX++) {
                        const int32_t targetX = destinationLeft + destinationX;
                        const int32_t sourceSampleOffsetX = (destinationX * sourceWidth) / destinationWidth;
                        const int32_t sampleX = std::clamp<int32_t>(
                            sourceLeft + sourceSampleOffsetX,
                            0,
                            static_cast<int32_t>(atlasWidth - 1));
                        std::uint32_t tintedPixel = MultiplyAbgrColor(
                            atlasPixels[(sampleY * atlasWidth) + sampleX],
                            cacheEntry.Color);
                        BlendAbgrPixel(
                            tintedPixel,
                            surfacePixels[(targetY * textureWidth) + targetX]);
                    }
                }
            }

            const double advanceWidth = glyph.AdvanceWidth > 0.0f
                ? glyph.AdvanceWidth * fontScale
                : glyphWidth;
            offsetX += advanceWidth;
        }

        ReleaseStaticTextSurface(cacheEntry);
        cacheEntry.StaticSurfaceTexture = new PspRuntimeTexture();
        cacheEntry.StaticSurfaceTexture->set_Id("engine:psp:static-text-surface");
        cacheEntry.StaticSurfaceTexture->set_Width(textureWidth);
        cacheEntry.StaticSurfaceTexture->set_Height(textureHeight);
        cacheEntry.StaticSurfaceTexture->SetPixelsAbgr8888(std::move(surfacePixels));
        cacheEntry.StaticSurfacePosition = float3(minimumX, minimumY, 0.0f);
        cacheEntry.StaticSurfaceSize = int2(visibleSurfaceWidth, visibleSurfaceHeight);
        cacheEntry.StaticSurfaceSourceRect = float4(
            0.0f,
            0.0f,
            static_cast<float>(visibleSurfaceWidth) / static_cast<float>(textureWidth),
            static_cast<float>(visibleSurfaceHeight) / static_cast<float>(textureHeight));
    }

    /// Releases one cached static text surface texture when the cache entry changes ownership.
    void PspRenderManager2D::ReleaseStaticTextSurface(TextGeometryCacheEntry& cacheEntry) {
        if (cacheEntry.StaticSurfaceTexture != nullptr) {
            delete cacheEntry.StaticSurfaceTexture;
            cacheEntry.StaticSurfaceTexture = nullptr;
        }

        cacheEntry.StaticSurfacePosition = float3();
        cacheEntry.StaticSurfaceSize = int2();
        cacheEntry.StaticSurfaceSourceRect = float4();
    }

    /// Clears all cached 2D geometry so subsequent draws rebuild from current authored state.
    void PspRenderManager2D::ClearGeometryCaches() {
        for (TextGeometryCacheEntry& entry : TextGeometryCacheEntries) {
            ReleaseStaticTextSurface(entry);
        }
        TextGeometryCacheEntries.clear();
        RoundedRectGeometryCacheEntries.clear();
        PendingWhiteTriangles.clear();
    }

    /// Draws one textured screen-space quad using PSP 2D texel-space UV coordinates.
    void PspRenderManager2D::DrawTexturedQuad(RuntimeTexture* texture, const float3& position, const int2& size, const float4& sourceRect, const byte4& color) {
        const std::uint64_t drawStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        if (texture == nullptr || size.X <= 0 || size.Y <= 0) {
            return;
        }

        PspRuntimeTexture* pspTexture = dynamic_cast<PspRuntimeTexture*>(texture);
        if (pspTexture == nullptr) {
            throw std::runtime_error("PSP 2D textures must be PSP runtime textures.");
        }

        if (pspTexture != GetWhiteTexture()) {
            FlushPendingWhiteTriangles();
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

        const std::uint64_t drawArrayStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        sceGuDrawArray(
            GU_SPRITES,
            GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
            2,
            nullptr,
            vertices);
        const std::uint64_t drawArrayMicroseconds = PspRenderProfiler::GetTimestampMicroseconds() - drawArrayStartMicroseconds;
        PspRenderProfiler::Record2DTexturedQuad(
            pspTexture,
            size,
            PspRenderProfiler::GetTimestampMicroseconds() - drawStartMicroseconds,
            drawArrayMicroseconds);
    }

    /// Draws one textured screen-space quad after applying authored scale and rotation around its center.
    void PspRenderManager2D::DrawTexturedQuadTransformed(RuntimeTexture* texture, const float3& position, float width, float height, const float4& sourceRect, const byte4& color, float rotationRadians) {
        const double absoluteWidth = std::abs(static_cast<double>(width));
        const double absoluteHeight = std::abs(static_cast<double>(height));
        if (texture == nullptr || absoluteWidth <= 0.0001d || absoluteHeight <= 0.0001d) {
            return;
        }

        if (std::abs(static_cast<double>(rotationRadians)) <= 0.0001d) {
            DrawTexturedQuad(
                texture,
                position,
                int2(
                    static_cast<int32_t>(std::lround(width)),
                    static_cast<int32_t>(std::lround(height))),
                sourceRect,
                color);
            return;
        }

        const std::uint32_t packedColor = ConvertColorToAbgr(color);
        const float4 textureSourceRect = ConvertSourceRectToTexturePixels(sourceRect, texture);
        const double centerX = static_cast<double>(position.X) + (static_cast<double>(width) * 0.5d);
        const double centerY = static_cast<double>(position.Y) + (static_cast<double>(height) * 0.5d);
        const double halfWidth = static_cast<double>(width) * 0.5d;
        const double halfHeight = static_cast<double>(height) * 0.5d;
        const double rotationSin = std::sin(static_cast<double>(rotationRadians));
        const double rotationCos = std::cos(static_cast<double>(rotationRadians));

        const double localLeft = -halfWidth;
        const double localRight = halfWidth;
        const double localTop = -halfHeight;
        const double localBottom = halfHeight;

        Psp2DVertex vertices[6] = {};
        vertices[0].U = textureSourceRect.X;
        vertices[0].V = textureSourceRect.Y;
        vertices[0].Color = packedColor;
        vertices[0].X = static_cast<float>(centerX + (localLeft * rotationCos) - (localTop * rotationSin));
        vertices[0].Y = static_cast<float>(centerY + (localLeft * rotationSin) + (localTop * rotationCos));
        vertices[0].Z = position.Z;

        vertices[1].U = textureSourceRect.X + textureSourceRect.Z;
        vertices[1].V = textureSourceRect.Y;
        vertices[1].Color = packedColor;
        vertices[1].X = static_cast<float>(centerX + (localRight * rotationCos) - (localTop * rotationSin));
        vertices[1].Y = static_cast<float>(centerY + (localRight * rotationSin) + (localTop * rotationCos));
        vertices[1].Z = position.Z;

        vertices[2].U = textureSourceRect.X + textureSourceRect.Z;
        vertices[2].V = textureSourceRect.Y + textureSourceRect.W;
        vertices[2].Color = packedColor;
        vertices[2].X = static_cast<float>(centerX + (localRight * rotationCos) - (localBottom * rotationSin));
        vertices[2].Y = static_cast<float>(centerY + (localRight * rotationSin) + (localBottom * rotationCos));
        vertices[2].Z = position.Z;

        vertices[3].U = textureSourceRect.X;
        vertices[3].V = textureSourceRect.Y;
        vertices[3].Color = packedColor;
        vertices[3].X = vertices[0].X;
        vertices[3].Y = vertices[0].Y;
        vertices[3].Z = position.Z;

        vertices[4].U = textureSourceRect.X + textureSourceRect.Z;
        vertices[4].V = textureSourceRect.Y + textureSourceRect.W;
        vertices[4].Color = packedColor;
        vertices[4].X = vertices[2].X;
        vertices[4].Y = vertices[2].Y;
        vertices[4].Z = position.Z;

        vertices[5].U = textureSourceRect.X;
        vertices[5].V = textureSourceRect.Y + textureSourceRect.W;
        vertices[5].Color = packedColor;
        vertices[5].X = static_cast<float>(centerX + (localLeft * rotationCos) - (localBottom * rotationSin));
        vertices[5].Y = static_cast<float>(centerY + (localLeft * rotationSin) + (localBottom * rotationCos));
        vertices[5].Z = position.Z;

        DrawTexturedTriangles(vertices, 6, texture);
    }

    /// Draws one textured 2D triangle list using the PSP GU textured vertex path.
    void PspRenderManager2D::DrawTexturedTriangles(const Psp2DVertex* vertices, std::size_t vertexCount, RuntimeTexture* texture) {
        const std::uint64_t drawStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        if (vertices == nullptr || vertexCount < 3 || texture == nullptr) {
            return;
        }

        PspRuntimeTexture* pspTexture = dynamic_cast<PspRuntimeTexture*>(texture);
        if (pspTexture == nullptr) {
            throw std::runtime_error("PSP 2D triangle draws must use PSP runtime textures.");
        }

        if (pspTexture != GetWhiteTexture()) {
            FlushPendingWhiteTriangles();
        }

        BindTexture(pspTexture);
        sceGuDisable(GU_DEPTH_TEST);
        Psp2DVertex* drawVertices = static_cast<Psp2DVertex*>(sceGuGetMemory(sizeof(Psp2DVertex) * vertexCount));
        std::copy(vertices, vertices + vertexCount, drawVertices);

        const std::uint64_t drawArrayStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        sceGuDrawArray(
            GU_TRIANGLES,
            GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
            static_cast<int>(vertexCount),
            nullptr,
            drawVertices);
        const std::uint64_t drawArrayMicroseconds = PspRenderProfiler::GetTimestampMicroseconds() - drawArrayStartMicroseconds;
        PspRenderProfiler::Record2DTexturedTriangles(
            pspTexture,
            vertexCount,
            PspRenderProfiler::GetTimestampMicroseconds() - drawStartMicroseconds,
            drawArrayMicroseconds);
    }

    /// Draws one textured 2D triangle list after applying one world-space offset to cached local vertices.
    void PspRenderManager2D::DrawTexturedTrianglesTranslated(const Psp2DVertex* vertices, std::size_t vertexCount, RuntimeTexture* texture, const float3& positionOffset) {
        const std::uint64_t drawStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        if (vertices == nullptr || vertexCount < 3 || texture == nullptr) {
            return;
        }

        PspRuntimeTexture* pspTexture = dynamic_cast<PspRuntimeTexture*>(texture);
        if (pspTexture == nullptr) {
            throw std::runtime_error("PSP 2D triangle draws must use PSP runtime textures.");
        }

        if (pspTexture != GetWhiteTexture()) {
            FlushPendingWhiteTriangles();
        }

        BindTexture(pspTexture);
        sceGuDisable(GU_DEPTH_TEST);
        Psp2DVertex* drawVertices = static_cast<Psp2DVertex*>(sceGuGetMemory(sizeof(Psp2DVertex) * vertexCount));
        for (std::size_t index = 0; index < vertexCount; index++) {
            drawVertices[index] = vertices[index];
            drawVertices[index].X += positionOffset.X;
            drawVertices[index].Y += positionOffset.Y;
            drawVertices[index].Z += positionOffset.Z;
        }

        const std::uint64_t drawArrayStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        sceGuDrawArray(
            GU_TRIANGLES,
            GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
            static_cast<int>(vertexCount),
            nullptr,
            drawVertices);
        const std::uint64_t drawArrayMicroseconds = PspRenderProfiler::GetTimestampMicroseconds() - drawArrayStartMicroseconds;
        PspRenderProfiler::Record2DTexturedTriangles(
            pspTexture,
            vertexCount,
            PspRenderProfiler::GetTimestampMicroseconds() - drawStartMicroseconds,
            drawArrayMicroseconds);
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

        return true;
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

        FlushPendingWhiteTriangles();

        const int left = std::clamp(static_cast<int>(std::floor(clipRect.X)), 0, PspScreenWidth);
        const int top = std::clamp(static_cast<int>(std::floor(clipRect.Y)), 0, PspScreenHeight);
        const int right = std::clamp(static_cast<int>(std::ceil(clipRect.X + clipRect.Z)), left, PspScreenWidth);
        const int bottom = std::clamp(static_cast<int>(std::ceil(clipRect.Y + clipRect.W)), top, PspScreenHeight);
        const int width = std::max(0, right - left);
        const int height = std::max(0, bottom - top);

        sceGuEnable(GU_SCISSOR_TEST);
        sceGuScissor(left, top, width, height);

        HasActiveClipRect = true;
        ActiveClipRect = clipRect;
    }

    /// Clears the drawable clip rectangle so subsequent 2D draws are unrestricted.
    void PspRenderManager2D::ClearClipRect() {
        FlushPendingWhiteTriangles();
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

    /// Modulates one ABGR8888 source pixel by the supplied drawable tint.
    std::uint32_t PspRenderManager2D::MultiplyAbgrColor(std::uint32_t sourcePixel, const byte4& tint) {
        const std::uint32_t sourceRed = sourcePixel & 0xffu;
        const std::uint32_t sourceGreen = (sourcePixel >> 8) & 0xffu;
        const std::uint32_t sourceBlue = (sourcePixel >> 16) & 0xffu;
        const std::uint32_t sourceAlpha = (sourcePixel >> 24) & 0xffu;
        const std::uint32_t multipliedRed = (sourceRed * tint.X) / 255u;
        const std::uint32_t multipliedGreen = (sourceGreen * tint.Y) / 255u;
        const std::uint32_t multipliedBlue = (sourceBlue * tint.Z) / 255u;
        const std::uint32_t multipliedAlpha = (sourceAlpha * tint.W) / 255u;

        return (multipliedAlpha << 24)
            | (multipliedBlue << 16)
            | (multipliedGreen << 8)
            | multipliedRed;
    }

    /// Blends one ABGR8888 source pixel over one destination pixel using standard source-alpha composition.
    void PspRenderManager2D::BlendAbgrPixel(std::uint32_t sourcePixel, std::uint32_t& destinationPixel) {
        const std::uint32_t sourceAlpha = (sourcePixel >> 24) & 0xffu;
        if (sourceAlpha == 0u) {
            return;
        }

        if (sourceAlpha == 255u) {
            destinationPixel = sourcePixel;
            return;
        }

        const std::uint32_t destinationRed = destinationPixel & 0xffu;
        const std::uint32_t destinationGreen = (destinationPixel >> 8) & 0xffu;
        const std::uint32_t destinationBlue = (destinationPixel >> 16) & 0xffu;
        const std::uint32_t destinationAlpha = (destinationPixel >> 24) & 0xffu;
        const std::uint32_t sourceRed = sourcePixel & 0xffu;
        const std::uint32_t sourceGreen = (sourcePixel >> 8) & 0xffu;
        const std::uint32_t sourceBlue = (sourcePixel >> 16) & 0xffu;
        const std::uint32_t inverseSourceAlpha = 255u - sourceAlpha;
        const std::uint32_t blendedRed = sourceRed + ((destinationRed * inverseSourceAlpha) / 255u);
        const std::uint32_t blendedGreen = sourceGreen + ((destinationGreen * inverseSourceAlpha) / 255u);
        const std::uint32_t blendedBlue = sourceBlue + ((destinationBlue * inverseSourceAlpha) / 255u);
        const std::uint32_t blendedAlpha = sourceAlpha + ((destinationAlpha * inverseSourceAlpha) / 255u);

        destinationPixel = (std::min<std::uint32_t>(255u, blendedAlpha) << 24)
            | (std::min<std::uint32_t>(255u, blendedBlue) << 16)
            | (std::min<std::uint32_t>(255u, blendedGreen) << 8)
            | std::min<std::uint32_t>(255u, blendedRed);
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
