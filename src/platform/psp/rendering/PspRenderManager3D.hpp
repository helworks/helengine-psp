#pragma once

#include <vector>

#include "ICamera.hpp"
#include "IDrawable3D.hpp"
#include "IContentStreamSource.hpp"
#include "IRenderVisitor3D.hpp"
#include "PlatformMaterialAsset.hpp"
#include "RenderManager3D.hpp"
#include "RuntimeMaterial.hpp"
#include "RuntimeModel.hpp"
#include "float3.hpp"
#include "float4x4.hpp"
#include "platform/psp/rendering/PspLightingSettings.hpp"
#include "platform/psp/rendering/PspRenderManager2D.hpp"
#include "platform/psp/rendering/PspRenderProfiler.hpp"
#include "platform/psp/rendering/PspRuntimeModel.hpp"
#include "platform/psp/rendering/PspSceneLightingSnapshot.hpp"

namespace helengine::psp::rendering {
    /// Accepts generated-core 3D drawables and renders PSP 3D meshes through the active lighting pipeline.
    class PspRenderManager3D final : public RenderManager3D, public IRenderVisitor3D {
    public:
        /// Creates the PSP 3D render manager.
        PspRenderManager3D();

        /// Releases renderer-owned transient vertex buffers after the final PSP frame has completed.
        ~PspRenderManager3D() override;

        /// Builds a CPU-side runtime model payload from the raw mesh asset.
        RuntimeModel* BuildModelFromRaw(ModelAsset* data) override;

        /// Loads a cooked model payload from packaged PSP content and builds its fixed-function runtime representation.
        RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override;

        /// Builds one fixed-function PSP runtime material from its cooked platform payload.
        RuntimeMaterial* BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset) override;

        /// Loads and builds one fixed-function PSP runtime material from its cooked content-relative path.
        RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override;

        /// Releases one PSP runtime model after the final scene reference is removed.
        void ReleaseModel(RuntimeModel* model) override;

        /// Releases one PSP runtime material after the final scene reference is removed.
        void ReleaseMaterial(RuntimeMaterial* material) override;

        /// Wires the paired PSP 2D renderer used for per-camera UI submission.
        void SetRenderManager2D(PspRenderManager2D* renderManager2D);

        /// Arms a bounded renderer trace for the first frames submitted after physics scene binding completes.
        static void BeginPostPhysicsBindingDrawTrace();

        /// Draws every visible authored camera to the current PSP back buffer.
        void Draw() override;

        /// Draws one queued mesh for the active camera.
        void Visit(IDrawable3D* drawable) override;

    private:
        /// Releases the scaled fixed-function vertex streams retained until the preceding GU frame is synchronized.
        void ReleaseFrameScaledFixedFunctionVertexBuffers();

        /// Builds and retains one heap-backed fixed-function vertex stream with positions scaled for non-uniform GPU lighting.
        PspRuntimeModel::FixedFunctionVertex* CreateScaledFixedFunctionVertices(
            const PspRuntimeModel* runtimeModel,
            const float3& scale);

        /// Builds and retains one heap-backed textured vertex stream until the PSP has consumed the active GU display list.
        PspRuntimeModel::FixedFunctionTexturedVertex* CreateTransientFixedFunctionTexturedVertices(
            const PspRuntimeModel* runtimeModel,
            PspRuntimeTexture* texture,
            const float3* scale);

        /// Releases textured fixed-function vertex streams retained until the preceding GU frame is synchronized.
        void ReleaseFrameTransientFixedFunctionTexturedVertexBuffers();

        /// Writes one renderer trace record while a post-physics-binding trace is active.
        static void WritePostPhysicsBindingDrawTrace(const std::string& stage);

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

        /// Stores the number of fully rendered frames that should continue emitting post-physics-binding trace records.
        static int32_t PostPhysicsBindingTraceFramesRemaining;

        /// Stores heap-backed scaled vertex streams until the PSP has consumed the active GU display list.
        std::vector<PspRuntimeModel::FixedFunctionVertex*> FrameScaledFixedFunctionVertexBuffers;

        /// Stores heap-backed textured vertex streams until the PSP has consumed the active GU display list.
        std::vector<PspRuntimeModel::FixedFunctionTexturedVertex*> FrameTransientFixedFunctionTexturedVertexBuffers;
    };
}
