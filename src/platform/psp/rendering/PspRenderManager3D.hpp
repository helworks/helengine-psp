#pragma once

#include "ICamera.hpp"
#include "IDrawable3D.hpp"
#include "IRenderVisitor3D.hpp"
#include "MaterialAsset.hpp"
#include "RenderManager3D.hpp"
#include "RuntimeMaterial.hpp"
#include "RuntimeModel.hpp"
#include "ShaderAsset.hpp"
#include "float3.hpp"
#include "float4x4.hpp"
#include "platform/psp/rendering/PspLightingSettings.hpp"
#include "platform/psp/rendering/PspRenderManager2D.hpp"
#include "platform/psp/rendering/PspSceneLightingSnapshot.hpp"

namespace helengine::psp::rendering {
    /// Accepts generated-core 3D drawables and renders PSP 3D meshes through the active lighting pipeline.
    class PspRenderManager3D final : public RenderManager3D, public IRenderVisitor3D {
    public:
        /// Creates the PSP 3D render manager.
        PspRenderManager3D();

        /// Builds a CPU-side runtime model payload from the raw mesh asset.
        RuntimeModel* BuildModelFromRaw(ModelAsset* data) override;

        /// Builds a runtime material placeholder and captures the authored base color.
        RuntimeMaterial* BuildMaterialFromRaw(MaterialAsset* materialAsset, ShaderAsset* shaderAsset) override;

        /// Wires the paired PSP 2D renderer used for per-camera UI submission.
        void SetRenderManager2D(PspRenderManager2D* renderManager2D);

        /// Draws every visible authored camera to the current PSP back buffer.
        void Draw() override;

        /// Draws one queued mesh for the active camera.
        void Visit(IDrawable3D* drawable) override;

    private:
        /// Resolves the active scene lighting for the current render pass.
        void ResolveSceneLighting();

        /// Renders the currently active 3D queue for one camera.
        void RenderCamera(ICamera* camera);

        /// Stores the renderer-owned PSP lighting settings.
        PspLightingSettings LightingSettings;

        /// Stores the active scene-light snapshot for the current render pass.
        PspSceneLightingSnapshot CurrentLighting;

        /// Stores the active camera view matrix for the current pass.
        float4x4 CurrentView;

        /// Stores the active camera projection matrix for the current pass.
        float4x4 CurrentProjection;

        /// Stores the active camera world-space position.
        float3 CurrentCameraPosition;

        /// Stores the paired PSP 2D renderer that submits the camera's 2D queue.
        PspRenderManager2D* RenderManager2D;
    };
}
