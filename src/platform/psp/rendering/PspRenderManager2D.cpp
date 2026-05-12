#include "platform/psp/rendering/PspRenderManager2D.hpp"

namespace helengine::psp::rendering {
    /// Builds a PSP runtime texture from raw texture data and reuses cached instances by asset id.
    RuntimeTexture* PspRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        return TextureCache.BuildTextureFromRaw(data);
    }

    /// Ignores rounded-rectangle draws during the first 3D-only PSP milestone.
    void PspRenderManager2D::DrawRoundedRect(IRoundedRectDrawable2D* shape) {
        (void)shape;
    }

    /// Ignores sprite draws during the first 3D-only PSP milestone.
    void PspRenderManager2D::DrawSprite(ISpriteDrawable2D* sprite) {
        (void)sprite;
    }

    /// Ignores text draws during the first 3D-only PSP milestone.
    void PspRenderManager2D::DrawText(ITextDrawable2D* text) {
        (void)text;
    }
}
