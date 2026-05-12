#include "platform/psp/rendering/PspRenderManager3D.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <pspgu.h>
#include <pspgum.h>

#include "Core.hpp"
#include "DirectionalLightComponent.hpp"
#include "Entity.hpp"
#include "IRenderQueue3D.hpp"
#include "ModelAsset.hpp"
#include "ObjectManager.hpp"
#include "platform/psp/PspBootTrace.hpp"
#include "platform/psp/rendering/PspRuntimeMaterial.hpp"
#include "platform/psp/rendering/PspRuntimeModel.hpp"

namespace helengine::psp::rendering {
    namespace {
        /// Stores one raw PSP mesh payload keyed by the generated runtime-model instance.
        struct PspMeshRecord {
            std::vector<float3> Positions;
            std::vector<float3> Normals;
            std::vector<std::uint32_t> Indices;
        };

        /// Stores one GU vertex with per-vertex color and position.
        struct PspLitVertex {
            std::uint32_t Color;
            float X;
            float Y;
            float Z;
        };

        /// Stores one raw 4x4 float buffer that can be reinterpreted as a PSP GU matrix.
        struct alignas(16) PspMatrixBuffer {
            float M[4][4];
        };

        std::unordered_map<const RuntimeModel*, PspMeshRecord> MeshRecords;
        int LoggedFrameCount = 0;
        int CurrentFrameVisitedDrawables = 0;
        int CurrentFrameSubmittedVertices = 0;

        /// Determines whether the current frame should emit bring-up render diagnostics.
        bool ShouldLogRenderDiagnostics() {
            return LoggedFrameCount < 8;
        }

        /// Converts one generated matrix into the column-major layout expected by PSP GU.
        PspMatrixBuffer CreatePspMatrixBuffer(const float4x4& matrix) {
            PspMatrixBuffer buffer {};
            buffer.M[0][0] = matrix.M11;
            buffer.M[0][1] = matrix.M12;
            buffer.M[0][2] = matrix.M13;
            buffer.M[0][3] = matrix.M14;
            buffer.M[1][0] = matrix.M21;
            buffer.M[1][1] = matrix.M22;
            buffer.M[1][2] = matrix.M23;
            buffer.M[1][3] = matrix.M24;
            buffer.M[2][0] = matrix.M31;
            buffer.M[2][1] = matrix.M32;
            buffer.M[2][2] = matrix.M33;
            buffer.M[2][3] = matrix.M34;
            buffer.M[3][0] = matrix.M41;
            buffer.M[3][1] = matrix.M42;
            buffer.M[3][2] = matrix.M43;
            buffer.M[3][3] = matrix.M44;
            return buffer;
        }

        /// Converts one authored linear color into the PSP GU ABGR8888 format.
        std::uint32_t ClampColorChannel(float value) {
            float clamped = std::min(std::max(value, 0.0f), 1.0f);
            return static_cast<std::uint32_t>(std::lround(clamped * 255.0f));
        }

        /// Converts one authored linear color into the PSP GU ABGR8888 format.
        std::uint32_t ConvertColorToAbgr(const float4& color) {
            std::uint32_t red = ClampColorChannel(color.X);
            std::uint32_t green = ClampColorChannel(color.Y);
            std::uint32_t blue = ClampColorChannel(color.Z);
            std::uint32_t alpha = ClampColorChannel(color.W);
            return (alpha << 24) | (blue << 16) | (green << 8) | red;
        }

        /// Builds one world matrix from the generated entity transform.
        float4x4 BuildWorldMatrix(Entity* entity) {
            if (entity == nullptr) {
                return float4x4::get_Identity();
            }

            float4 orientation = entity->get_Orientation();
            float3 scale = entity->get_Scale();
            float3 position = entity->get_Position();

            float4x4 rotation;
            float4x4::CreateFromQuaternion(orientation, rotation);

            float4x4 size;
            float4x4::CreateScale(scale.X, scale.Y, scale.Z, size);

            float4x4 rotationScale;
            float4x4::Multiply(rotation, size, rotationScale);

            float4x4 translation;
            float4x4::CreateTranslation(position, translation);

            float4x4 world;
            float4x4::Multiply(rotationScale, translation, world);
            return world;
        }

        /// Rotates one local-space normal into world space using the entity orientation.
        float3 RotateNormal(const float3& normal, Entity* entity) {
            if (entity == nullptr) {
                return normal;
            }

            return float4::RotateVector(normal, entity->get_Orientation());
        }

        /// Clamps one floating-point color to the normalized channel range expected before PSP packing.
        float4 ClampColor(const float4& color) {
            return float4(
                std::min(std::max(color.X, 0.0f), 1.0f),
                std::min(std::max(color.Y, 0.0f), 1.0f),
                std::min(std::max(color.Z, 0.0f), 1.0f),
                std::min(std::max(color.W, 0.0f), 1.0f));
        }
    }

    /// Creates the PSP 3D render manager.
    PspRenderManager3D::PspRenderManager3D()
        : CurrentView(float4x4::get_Identity()),
          CurrentProjection(float4x4::get_Identity()),
          CurrentCameraPosition(0.0f, 0.0f, 0.0f) {
    }

    /// Builds a CPU-side runtime model payload from the raw mesh asset.
    RuntimeModel* PspRenderManager3D::BuildModelFromRaw(ModelAsset* data) {
        PspRuntimeModel* runtimeModel = new PspRuntimeModel();
        PspMeshRecord record;

        if (data != nullptr) {
            runtimeModel->set_Id(data->get_Id());
            if (data->Positions != nullptr && data->Positions->Length > 0) {
                record.Positions.reserve(static_cast<std::size_t>(data->Positions->Length));
                for (int32_t index = 0; index < data->Positions->Length; index++) {
                    record.Positions.push_back((*data->Positions)[index]);
                }
            }

            if (data->Normals != nullptr && data->Normals->Length > 0) {
                record.Normals.reserve(static_cast<std::size_t>(data->Normals->Length));
                for (int32_t index = 0; index < data->Normals->Length; index++) {
                    record.Normals.push_back((*data->Normals)[index]);
                }
            }

            if (data->Indices32 != nullptr && data->Indices32->Length > 0) {
                record.Indices.reserve(static_cast<std::size_t>(data->Indices32->Length));
                for (int32_t index = 0; index < data->Indices32->Length; index++) {
                    record.Indices.push_back((*data->Indices32)[index]);
                }
            } else if (data->Indices16 != nullptr && data->Indices16->Length > 0) {
                record.Indices.reserve(static_cast<std::size_t>(data->Indices16->Length));
                for (int32_t index = 0; index < data->Indices16->Length; index++) {
                    record.Indices.push_back((*data->Indices16)[index]);
                }
            }
        }

        MeshRecords[runtimeModel] = record;
        return runtimeModel;
    }

    /// Builds a runtime material placeholder and captures the authored base color.
    RuntimeMaterial* PspRenderManager3D::BuildMaterialFromRaw(MaterialAsset* materialAsset, ShaderAsset* shaderAsset) {
        PspRuntimeMaterial* runtimeMaterial = new PspRuntimeMaterial();
        if (materialAsset != nullptr) {
            runtimeMaterial->LoadFromCooked(materialAsset);
        } else if (shaderAsset != nullptr) {
            runtimeMaterial->set_Id(shaderAsset->get_Id());
        }

        return runtimeMaterial;
    }

    /// Draws every visible authored camera to the current PSP back buffer.
    void PspRenderManager3D::Draw() {
        RenderManager3D::Draw();
        if (Core::get_Instance() == nullptr || Core::get_Instance()->get_ObjectManager() == nullptr) {
            return;
        }

        List<ICamera*>* cameras = Core::get_Instance()->get_ObjectManager()->get_Cameras();
        if (cameras == nullptr || cameras->Count() == 0) {
            if (ShouldLogRenderDiagnostics()) {
                PspBootTrace::WriteLine("Render3D Draw cameras=0");
                LoggedFrameCount++;
            }
            return;
        }

        if (ShouldLogRenderDiagnostics()) {
            std::string windowSizeText = "none";
            if (get_MainWindowSize() != nullptr) {
                windowSizeText = std::to_string(get_MainWindowSize()->X) + "x" + std::to_string(get_MainWindowSize()->Y);
            }

            PspBootTrace::WriteLine(
                "Render3D Draw cameras=" + std::to_string(cameras->Count())
                + " window=" + windowSizeText);
        }

        for (int32_t index = 0; index < cameras->Count(); index++) {
            ICamera* camera = (*cameras)[index];
            if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }
            if (camera->get_RenderTarget() != nullptr) {
                continue;
            }

            RenderCamera(camera);
        }

        if (ShouldLogRenderDiagnostics()) {
            LoggedFrameCount++;
        }
    }

    /// Draws one queued mesh for the active camera.
    void PspRenderManager3D::Visit(IDrawable3D* drawable) {
        if (drawable == nullptr || drawable->get_Parent() == nullptr || !drawable->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        RuntimeModel* runtimeModel = drawable->get_Model();
        if (runtimeModel == nullptr) {
            return;
        }

        CurrentFrameVisitedDrawables++;

        auto meshRecordIterator = MeshRecords.find(runtimeModel);
        if (meshRecordIterator == MeshRecords.end()) {
            if (ShouldLogRenderDiagnostics()) {
                PspBootTrace::WriteLine("Render3D Visit missing-mesh-record");
            }
            return;
        }

        const PspMeshRecord& record = meshRecordIterator->second;
        if (record.Positions.empty()) {
            return;
        }

        RuntimeMaterial* runtimeMaterial = drawable->get_Material();
        RuntimeMaterial* rootMaterial = runtimeMaterial != nullptr ? runtimeMaterial->ResolveRootMaterial() : nullptr;
        PspRuntimeMaterial* pspRuntimeMaterial = dynamic_cast<PspRuntimeMaterial*>(rootMaterial);
        if (pspRuntimeMaterial == nullptr) {
            return;
        }

        if (LightingSettings.Pipeline == PspLightingPipeline::FixedFunctionLambert) {
            throw std::runtime_error("PspLightingPipeline::FixedFunctionLambert is not implemented yet.");
        }

        float4x4 world = BuildWorldMatrix(drawable->get_Parent());
        PspMatrixBuffer worldMatrix = CreatePspMatrixBuffer(world);

        sceGumMatrixMode(GU_MODEL);
        sceGumLoadMatrix(reinterpret_cast<ScePspFMatrix4*>(&worldMatrix));

        int32_t vertexCount = static_cast<int32_t>(record.Indices.empty() ? record.Positions.size() : record.Indices.size());
        if (vertexCount < 3) {
            if (ShouldLogRenderDiagnostics()) {
                PspBootTrace::WriteLine("Render3D Visit vertexCount=" + std::to_string(vertexCount));
            }
            return;
        }

        CurrentFrameSubmittedVertices += vertexCount;

        const float4& baseColor = pspRuntimeMaterial->GetBaseColor();
        const bool useLighting = pspRuntimeMaterial->GetReceivesLighting()
            && pspRuntimeMaterial->GetLightingResponse() == PspMaterialLightingResponse::LitDirectional;

        PspLitVertex* vertices = static_cast<PspLitVertex*>(sceGuGetMemory(sizeof(PspLitVertex) * static_cast<std::size_t>(vertexCount)));
        for (int32_t index = 0; index < vertexCount; index++) {
            std::uint32_t sourceIndex = record.Indices.empty()
                ? static_cast<std::uint32_t>(index)
                : record.Indices[static_cast<std::size_t>(index)];
            if (sourceIndex >= record.Positions.size()) {
                return;
            }

            const float3& position = record.Positions[sourceIndex];
            const float3 sourceNormal = sourceIndex < record.Normals.size()
                ? record.Normals[sourceIndex]
                : float3(0.0f, 0.0f, 1.0f);
            const float3 worldNormal = float3::Normalize(RotateNormal(sourceNormal, drawable->get_Parent()));

            float4 litColor = baseColor;
            if (useLighting) {
                const float ambient = LightingSettings.AmbientIntensity;
                float diffuse = 0.0f;
                if (CurrentLighting.HasDirectionalLight) {
                    const float3 lightDirection = float3::Normalize(CurrentLighting.DirectionalLightDirection);
                    diffuse = std::max(0.0f, float3::Dot(worldNormal, lightDirection));
                }

                litColor = ClampColor(float4(
                    baseColor.X * (ambient + (diffuse * CurrentLighting.DirectionalLightColor.X * CurrentLighting.DirectionalLightIntensity)),
                    baseColor.Y * (ambient + (diffuse * CurrentLighting.DirectionalLightColor.Y * CurrentLighting.DirectionalLightIntensity)),
                    baseColor.Z * (ambient + (diffuse * CurrentLighting.DirectionalLightColor.Z * CurrentLighting.DirectionalLightIntensity)),
                    baseColor.W));
            }

            vertices[index] = PspLitVertex {
                ConvertColorToAbgr(litColor),
                position.X,
                position.Y,
                position.Z
            };
        }

        sceGumDrawArray(
            GU_TRIANGLES,
            GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
            vertexCount,
            nullptr,
            vertices);
    }

    /// Resolves the active scene lighting for the current render pass.
    void PspRenderManager3D::ResolveSceneLighting() {
        CurrentLighting = PspSceneLightingSnapshot();

        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr || core->get_ObjectManager()->get_Entities() == nullptr) {
            return;
        }

        List<Entity*>* entities = core->get_ObjectManager()->get_Entities();
        for (int32_t entityIndex = 0; entityIndex < entities->Count(); entityIndex++) {
            Entity* entity = (*entities)[entityIndex];
            if (entity == nullptr || entity->get_Components() == nullptr || !entity->get_IsHierarchyEnabled()) {
                continue;
            }

            List<Component*>* components = entity->get_Components();
            for (int32_t componentIndex = 0; componentIndex < components->Count(); componentIndex++) {
                DirectionalLightComponent* directionalLight = dynamic_cast<DirectionalLightComponent*>((*components)[componentIndex]);
                if (directionalLight == nullptr || directionalLight->get_Parent() == nullptr) {
                    continue;
                }

                CurrentLighting.HasDirectionalLight = true;
                CurrentLighting.DirectionalLightDirection = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), directionalLight->get_Parent()->get_Orientation());
                CurrentLighting.DirectionalLightColor = directionalLight->get_Color();
                CurrentLighting.DirectionalLightIntensity = directionalLight->get_Intensity();
                return;
            }
        }
    }

    /// Renders the currently active 3D queue for one camera.
    void PspRenderManager3D::RenderCamera(ICamera* camera) {
        Entity* cameraParent = camera->get_Parent();
        float3 cameraPosition = cameraParent->get_Position();
        float4 cameraOrientation = cameraParent->get_Orientation();
        float3 cameraForward = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), cameraOrientation);
        float3 cameraUp = float4::RotateVector(float3(0.0f, 1.0f, 0.0f), cameraOrientation);
        float3 cameraTarget = cameraPosition + cameraForward;
        CurrentCameraPosition = cameraPosition;

        float4x4::CreateLookAt(cameraPosition, cameraTarget, cameraUp, CurrentView);

        float aspectRatio = 1.0f;
        if (get_MainWindowSize() != nullptr && get_MainWindowSize()->Y > 0) {
            aspectRatio = static_cast<float>(get_MainWindowSize()->X) / static_cast<float>(get_MainWindowSize()->Y);
        }

        constexpr float CameraFieldOfViewRadians = 0.78539816339f;
        constexpr float NearPlaneDistance = 0.1f;
        constexpr float FarPlaneDistance = 100.0f;
        float4x4::CreatePerspectiveFieldOfView(CameraFieldOfViewRadians, aspectRatio, NearPlaneDistance, FarPlaneDistance, CurrentProjection);

        PspMatrixBuffer viewMatrix = CreatePspMatrixBuffer(CurrentView);
        PspMatrixBuffer projectionMatrix = CreatePspMatrixBuffer(CurrentProjection);

        sceGuDisable(GU_TEXTURE_2D);
        sceGuEnable(GU_DEPTH_TEST);
        sceGuDisable(GU_CULL_FACE);

        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadMatrix(reinterpret_cast<ScePspFMatrix4*>(&projectionMatrix));
        sceGumMatrixMode(GU_VIEW);
        sceGumLoadMatrix(reinterpret_cast<ScePspFMatrix4*>(&viewMatrix));

        IRenderQueue3D* renderQueue = camera->get_RenderQueue3D();
        if (renderQueue != nullptr) {
            CurrentFrameVisitedDrawables = 0;
            CurrentFrameSubmittedVertices = 0;
            ResolveSceneLighting();
            if (ShouldLogRenderDiagnostics()) {
                PspBootTrace::WriteLine(
                    "Render3D Camera queueCount=" + std::to_string(renderQueue->get_Count())
                    + " position="
                    + std::to_string(cameraPosition.X) + ","
                    + std::to_string(cameraPosition.Y) + ","
                    + std::to_string(cameraPosition.Z));
            }

            renderQueue->VisitOrdered(this);

            if (ShouldLogRenderDiagnostics()) {
                PspBootTrace::WriteLine(
                    "Render3D Camera visitedDrawables=" + std::to_string(CurrentFrameVisitedDrawables)
                    + " submittedVertices=" + std::to_string(CurrentFrameSubmittedVertices));
            }
        }
    }
}
