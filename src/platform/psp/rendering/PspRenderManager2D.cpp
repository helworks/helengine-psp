#include "platform/psp/rendering/PspRenderManager2D.hpp"

namespace helengine::psp::rendering {
    /// Builds a runtime texture placeholder from raw texture metadata.
    RuntimeTexture* PspRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        RuntimeTexture* runtimeTexture = new RuntimeTexture();
        if (data != nullptr) {
            runtimeTexture->set_Id(data->get_Id());
            runtimeTexture->set_Width(data->Width);
            runtimeTexture->set_Height(data->Height);
        }

        return runtimeTexture;
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
