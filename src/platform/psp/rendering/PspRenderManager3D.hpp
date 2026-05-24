#pragma once

class ContentManager;

#include "ICamera.hpp"
#include "IDrawable3D.hpp"
#include "IRenderVisitor3D.hpp"
#include "IShaderRenderManager3D.hpp"
#include "RenderManager3D.hpp"
#include "RuntimeMaterial.hpp"
#include "RuntimeModel.hpp"
#include "ShaderAsset.hpp"
#include "ShaderMaterialAsset.hpp"
#include "float3.hpp"
#include "float4x4.hpp"
#include "platform/psp/rendering/PspLightingSettings.hpp"
#include "platform/psp/rendering/PspRenderManager2D.hpp"
#include "platform/psp/rendering/PspRenderProfiler.hpp"
#include "platform/psp/rendering/PspSceneLightingSnapshot.hpp"

namespace helengine::psp::rendering {
    /// Accepts generated-core 3D drawables and renders PSP 3D meshes through the active lighting pipeline.
    class PspRenderManager3D final : public RenderManager3D, public IRenderVisitor3D, public IShaderRenderManager3D {
    public:
        /// Creates the PSP 3D render manager.
        PspRenderManager3D();

        /// Builds a CPU-side runtime model payload from the raw mesh asset.
        RuntimeModel* BuildModelFromRaw(ModelAsset* data) override;

        /// Builds one shader-backed runtime material from the packaged material asset path used by scene loading.
        RuntimeMaterial* BuildMaterialFromRawAsset(ContentManager* assetContentManager, std::string contentRootPath, std::string materialAssetPath) override;

        /// Builds a runtime material placeholder and captures the authored base color.
        RuntimeMaterial* BuildMaterialFromRaw(ShaderMaterialAsset* materialAsset, ShaderAsset* shaderAsset) override;

        /// Reports the shader target used to resolve packaged shader assets for PSP materials.
        ShaderCompileTarget get_ShaderCompileTarget() override;

        /// Invalidates shader-backed runtime resources when authored shader assets change.
        void InvalidateShaderResources(std::string shaderAssetId, ShaderAsset* shaderAsset) override;

        /// Releases one PSP runtime model after the final scene reference is removed.
        void ReleaseModel(RuntimeModel* model) override;

        /// Releases one PSP runtime material after the final scene reference is removed.
        void ReleaseMaterial(RuntimeMaterial* material) override;

        /// Wires the paired PSP 2D renderer used for per-camera UI submission.
        void SetRenderManager2D(PspRenderManager2D* renderManager2D);

        /// Draws every visible authored camera to the current PSP back buffer.
        void Draw() override;

        /// Draws one queued mesh for the active camera.
        void Visit(IDrawable3D* drawable) override;

    private:
        /// Resets the renderer-owned GU state cache before one camera pass begins.
        void ResetCachedGuState();

        /// Applies the requested PSP texturing state only when it differs from the active GU cache.
        void SetTextureEnabled(bool enabled);

        /// Applies the requested PSP lighting state only when it differs from the active GU cache.
        void SetLightingEnabled(bool enabled);

        /// Applies the requested PSP directional-light state only when it differs from the active GU cache.
        void SetLight0Enabled(bool enabled);

        /// Binds one PSP runtime texture for GU sampling or disables texturing when no texture exists.
        void BindTexture(class PspRuntimeTexture* texture);

        /// Configures the scene-wide fixed-function lighting state for the active camera pass.
        void ConfigureFixedFunctionSceneLighting();

        /// Configures the per-draw fixed-function material state for one PSP runtime material.
        void ConfigureFixedFunctionMaterial(const float4& baseColor, bool useLighting);

        /// Submits one drawable through the current fixed-function untextured lighting path.
        void SubmitFixedFunctionDrawable(
            const class PspRuntimeModel* runtimeModel,
            const float4& baseColor,
            bool useLighting,
            const float3* positionScale);

        /// Submits one drawable through the current fixed-function textured lighting path.
        void SubmitFixedFunctionTexturedDrawable(
            const class PspRuntimeModel* runtimeModel,
            const float4& baseColor,
            bool useLighting,
            class PspRuntimeTexture* texture,
            const float3* positionScale);

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

        /// Tracks whether the cached GU texturing state has been initialized for the current camera pass.
        bool HasCachedTextureEnabledState;

        /// Tracks the active GU texturing state for the current camera pass.
        bool CachedTextureEnabledState;

        /// Stores the currently bound PSP runtime texture for the current camera pass.
        class PspRuntimeTexture* CachedTexture;

        /// Tracks whether the cached GU lighting state has been initialized for the current camera pass.
        bool HasCachedLightingEnabledState;

        /// Tracks the active GU lighting state for the current camera pass.
        bool CachedLightingEnabledState;

        /// Tracks whether the cached GU directional-light state has been initialized for the current camera pass.
        bool HasCachedLight0EnabledState;

        /// Tracks the active GU directional-light state for the current camera pass.
        bool CachedLight0EnabledState;
    };
}
