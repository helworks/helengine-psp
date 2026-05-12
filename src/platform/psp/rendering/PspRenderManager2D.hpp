#pragma once

#include "RenderManager2D.hpp"
#include "RuntimeTexture.hpp"
#include "TextureAsset.hpp"
#include "platform/psp/rendering/PspTextureCache.hpp"

namespace helengine::psp::rendering {
    /// Provides the minimal 2D runtime surface required to initialize generated core on PSP.
    class PspRenderManager2D final : public RenderManager2D {
    public:
        /// Builds a runtime texture placeholder from raw texture metadata.
        RuntimeTexture* BuildTextureFromRaw(TextureAsset* data) override;

        /// Ignores rounded-rectangle draws during the first 3D-only PSP milestone.
        void DrawRoundedRect(IRoundedRectDrawable2D* shape) override;

        /// Ignores sprite draws during the first 3D-only PSP milestone.
        void DrawSprite(ISpriteDrawable2D* sprite) override;

        /// Ignores text draws during the first 3D-only PSP milestone.
        void DrawText(ITextDrawable2D* text) override;

    private:
        /// Stores cached PSP runtime textures built from cooked texture assets.
        PspTextureCache TextureCache;
    };
}
