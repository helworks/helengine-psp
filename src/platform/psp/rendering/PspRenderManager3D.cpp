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
#include <pspkernel.h>

#include "Core.hpp"
#include "DirectionalLightComponent.hpp"
#include "Entity.hpp"
#include "IRenderQueue3D.hpp"
#include "MaterialLayoutBuilder.hpp"
#include "ModelAsset.hpp"
#include "float2.hpp"
#include "ObjectManager.hpp"
#include "platform/psp/rendering/PspRuntimeMaterial.hpp"
#include "platform/psp/rendering/PspRuntimeModel.hpp"
#include "platform/psp/rendering/PspRuntimeTexture.hpp"

namespace helengine::psp::rendering {
    namespace {
        /// Stores one raw PSP mesh payload keyed by the generated runtime-model instance.
        struct PspMeshRecord {
            std::vector<float3> Positions;
            std::vector<float3> Normals;
            std::vector<float2> TexCoords;
            std::vector<std::uint32_t> Indices;
        };

        /// Stores one GU vertex with per-vertex color and position.
        struct PspLitVertex {
            std::uint32_t Color;
            float X;
            float Y;
            float Z;
        };

        /// Stores one GU textured vertex with UVs, per-vertex color, and position.
        struct PspTexturedLitVertex {
            float U;
            float V;
            std::uint32_t Color;
            float X;
            float Y;
            float Z;
        };

        /// Stores one GU fixed-function vertex with a local normal and position.
        struct PspFixedFunctionVertex {
            float NX;
            float NY;
            float NZ;
            float X;
            float Y;
            float Z;
        };

        /// Stores one GU fixed-function textured vertex with UVs, a local normal, and position.
        struct PspFixedFunctionTexturedVertex {
            float U;
            float V;
            float NX;
            float NY;
            float NZ;
            float X;
            float Y;
            float Z;
        };

        /// Stores one raw 4x4 float buffer that can be reinterpreted as a PSP GU matrix.
        struct alignas(16) PspMatrixBuffer {
            float M[4][4];
        };

        std::unordered_map<const RuntimeModel*, PspMeshRecord> MeshRecords;
        std::unordered_map<std::string, RuntimeModel*> CachedModels;

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

        /// Converts one generated vector into the PSP GU vector layout.
        ScePspFVector3 CreatePspVector3(const float3& value) {
            ScePspFVector3 result {};
            result.x = value.X;
            result.y = value.Y;
            result.z = value.Z;
            return result;
        }

        /// Returns whether one PSP material should react to the current directional-light path.
        bool UsesDirectionalLighting(PspRuntimeMaterial* material) {
            return material != nullptr
                && material->GetReceivesLighting()
                && material->GetLightingResponse() == PspMaterialLightingResponse::LitDirectional;
        }

        /// Evaluates the current CPU Lambert response for one vertex.
        float4 EvaluateCpuLitColor(
            const float4& baseColor,
            const float3& worldNormal,
            bool useLighting,
            const PspLightingSettings& lightingSettings,
            const PspSceneLightingSnapshot& lightingSnapshot) {
            if (!useLighting) {
                return baseColor;
            }

            const float ambient = lightingSettings.AmbientIntensity;
            float diffuse = 0.0f;
            if (lightingSnapshot.HasDirectionalLight) {
                const float3 lightDirection = float3::Normalize(lightingSnapshot.DirectionalLightDirection);
                diffuse = std::max(0.0f, float3::Dot(worldNormal, lightDirection));
            }

            return ClampColor(float4(
                baseColor.X * (ambient + (diffuse * lightingSnapshot.DirectionalLightColor.X * lightingSnapshot.DirectionalLightIntensity)),
                baseColor.Y * (ambient + (diffuse * lightingSnapshot.DirectionalLightColor.Y * lightingSnapshot.DirectionalLightIntensity)),
                baseColor.Z * (ambient + (diffuse * lightingSnapshot.DirectionalLightColor.Z * lightingSnapshot.DirectionalLightIntensity)),
                baseColor.W));
        }

        /// Configures the scene-wide fixed-function lighting state for the active camera pass.
        void ConfigureFixedFunctionSceneLighting(
            const PspLightingSettings& lightingSettings,
            const PspSceneLightingSnapshot& lightingSnapshot) {
            sceGuDisable(GU_TEXTURE_2D);
            sceGuLightMode(GU_SINGLE_COLOR);
            sceGuSpecular(0.0f);
            sceGuAmbient(ConvertColorToAbgr(float4(
                lightingSettings.AmbientIntensity,
                lightingSettings.AmbientIntensity,
                lightingSettings.AmbientIntensity,
                1.0f)));

            if (!lightingSnapshot.HasDirectionalLight) {
                sceGuDisable(GU_LIGHT0);
                return;
            }

            const float3 normalizedDirection = float3::Normalize(lightingSnapshot.DirectionalLightDirection);
            const float3 scaledDirectionalColor = float3(
                lightingSnapshot.DirectionalLightColor.X * lightingSnapshot.DirectionalLightIntensity,
                lightingSnapshot.DirectionalLightColor.Y * lightingSnapshot.DirectionalLightIntensity,
                lightingSnapshot.DirectionalLightColor.Z * lightingSnapshot.DirectionalLightIntensity);
            const ScePspFVector3 lightVector = CreatePspVector3(normalizedDirection);

            sceGuEnable(GU_LIGHT0);
            sceGuLight(0, GU_DIRECTIONAL, GU_DIFFUSE, &lightVector);
            sceGuLightAtt(0, 1.0f, 0.0f, 0.0f);
            sceGuLightColor(0, GU_AMBIENT, 0);
            sceGuLightColor(0, GU_DIFFUSE, ConvertColorToAbgr(float4(
                scaledDirectionalColor.X,
                scaledDirectionalColor.Y,
                scaledDirectionalColor.Z,
                1.0f)));
            sceGuLightColor(0, GU_SPECULAR, 0);
        }

        /// Configures the per-draw fixed-function material state for one PSP runtime material.
        void ConfigureFixedFunctionMaterial(const float4& baseColor, bool useLighting, bool hasDirectionalLight) {
            const std::uint32_t baseColorAbgr = ConvertColorToAbgr(baseColor);

            sceGuColor(baseColorAbgr);
            sceGuAmbientColor(baseColorAbgr);
            sceGuModelColor(0, baseColorAbgr, baseColorAbgr, 0);

            if (!useLighting) {
                sceGuDisable(GU_LIGHT0);
                sceGuDisable(GU_LIGHTING);
                return;
            }

            sceGuEnable(GU_LIGHTING);
            if (hasDirectionalLight) {
                sceGuEnable(GU_LIGHT0);
            } else {
                sceGuDisable(GU_LIGHT0);
            }
        }

        /// Submits one drawable through the existing CPU-lit vertex path.
        void SubmitCpuLitDrawable(
            IDrawable3D* drawable,
            const PspMeshRecord& record,
            const float4& baseColor,
            bool useLighting,
            const PspLightingSettings& lightingSettings,
            const PspSceneLightingSnapshot& lightingSnapshot) {
            int32_t vertexCount = static_cast<int32_t>(record.Indices.empty() ? record.Positions.size() : record.Indices.size());
            if (vertexCount < 3) {
                return;
            }

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
                const float4 litColor = EvaluateCpuLitColor(baseColor, worldNormal, useLighting, lightingSettings, lightingSnapshot);

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

        /// Submits one drawable through the current fixed-function untextured lighting path.
        void SubmitFixedFunctionDrawable(
            const PspMeshRecord& record,
            const float4& baseColor,
            bool useLighting,
            bool hasDirectionalLight) {
            int32_t vertexCount = static_cast<int32_t>(record.Indices.empty() ? record.Positions.size() : record.Indices.size());
            if (vertexCount < 3) {
                return;
            }

            BindTexture(nullptr);
            ConfigureFixedFunctionMaterial(baseColor, useLighting, hasDirectionalLight);

            PspFixedFunctionVertex* vertices = static_cast<PspFixedFunctionVertex*>(sceGuGetMemory(sizeof(PspFixedFunctionVertex) * static_cast<std::size_t>(vertexCount)));
            for (int32_t index = 0; index < vertexCount; index++) {
                std::uint32_t sourceIndex = record.Indices.empty()
                    ? static_cast<std::uint32_t>(index)
                    : record.Indices[static_cast<std::size_t>(index)];
                if (sourceIndex >= record.Positions.size()) {
                    throw std::runtime_error("PSP fixed-function draw submitted an out-of-range mesh index.");
                }

                const float3& position = record.Positions[sourceIndex];
                const float3 sourceNormal = sourceIndex < record.Normals.size()
                    ? record.Normals[sourceIndex]
                    : float3(0.0f, 0.0f, 1.0f);

                vertices[index] = PspFixedFunctionVertex {
                    sourceNormal.X,
                    sourceNormal.Y,
                    sourceNormal.Z,
                    position.X,
                    position.Y,
                    position.Z
                };
            }

            sceGumDrawArray(
                GU_TRIANGLES,
                GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                vertexCount,
                nullptr,
                vertices);
        }

        /// Submits one drawable through the current fixed-function textured lighting path.
        void SubmitFixedFunctionTexturedDrawable(
            const PspMeshRecord& record,
            const float4& baseColor,
            bool useLighting,
            bool hasDirectionalLight,
            PspRuntimeTexture* texture) {
            int32_t vertexCount = static_cast<int32_t>(record.Indices.empty() ? record.Positions.size() : record.Indices.size());
            if (vertexCount < 3) {
                return;
            }

            if (texture == nullptr || !texture->HasPixels()) {
                throw std::runtime_error("PSP fixed-function textured draws require a valid runtime texture.");
            } else if (record.TexCoords.empty()) {
                throw std::runtime_error("PSP fixed-function textured draws require mesh texcoords.");
            }

            BindTexture(texture);
            ConfigureFixedFunctionMaterial(baseColor, useLighting, hasDirectionalLight);

            PspFixedFunctionTexturedVertex* vertices = static_cast<PspFixedFunctionTexturedVertex*>(sceGuGetMemory(sizeof(PspFixedFunctionTexturedVertex) * static_cast<std::size_t>(vertexCount)));
            for (int32_t index = 0; index < vertexCount; index++) {
                std::uint32_t sourceIndex = record.Indices.empty()
                    ? static_cast<std::uint32_t>(index)
                    : record.Indices[static_cast<std::size_t>(index)];
                if (sourceIndex >= record.Positions.size() || sourceIndex >= record.TexCoords.size()) {
                    throw std::runtime_error("PSP fixed-function textured draw submitted an out-of-range mesh index.");
                }

                const float3& position = record.Positions[sourceIndex];
                const float3 sourceNormal = sourceIndex < record.Normals.size()
                    ? record.Normals[sourceIndex]
                    : float3(0.0f, 0.0f, 1.0f);
                const float2& texCoord = record.TexCoords[sourceIndex];

                vertices[index] = PspFixedFunctionTexturedVertex {
                    texCoord.X,
                    texCoord.Y,
                    sourceNormal.X,
                    sourceNormal.Y,
                    sourceNormal.Z,
                    position.X,
                    position.Y,
                    position.Z
                };
            }

            sceGumDrawArray(
                GU_TRIANGLES,
                GU_TEXTURE_32BITF | GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                vertexCount,
                nullptr,
                vertices);
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
        if (data != nullptr && !data->get_Id().empty()) {
            auto cachedModelIterator = CachedModels.find(data->get_Id());
            if (cachedModelIterator != CachedModels.end()) {
                return cachedModelIterator->second;
            }
        }

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

            if (data->TexCoords != nullptr && data->TexCoords->Length > 0) {
                record.TexCoords.reserve(static_cast<std::size_t>(data->TexCoords->Length));
                for (int32_t index = 0; index < data->TexCoords->Length; index++) {
                    record.TexCoords.push_back((*data->TexCoords)[index]);
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
        if (data != nullptr && !data->get_Id().empty()) {
            CachedModels.emplace(data->get_Id(), runtimeModel);
        }
        return runtimeModel;
    }

    /// Builds a runtime material placeholder and captures the authored base color.
    RuntimeMaterial* PspRenderManager3D::BuildMaterialFromRaw(MaterialAsset* materialAsset, ShaderAsset* shaderAsset) {
        PspRuntimeMaterial* runtimeMaterial = new PspRuntimeMaterial();
        if (materialAsset != nullptr) {
            runtimeMaterial->LoadFromCooked(materialAsset);
            runtimeMaterial->SetLayout(MaterialLayoutBuilder::Build(materialAsset, shaderAsset));
            runtimeMaterial->SetRenderState(materialAsset->RenderState);
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
            return;
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

        auto meshRecordIterator = MeshRecords.find(runtimeModel);
        if (meshRecordIterator == MeshRecords.end()) {
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

        float4x4 world = BuildWorldMatrix(drawable->get_Parent());
        PspMatrixBuffer worldMatrix = CreatePspMatrixBuffer(world);

        sceGumMatrixMode(GU_MODEL);
        sceGumLoadMatrix(reinterpret_cast<ScePspFMatrix4*>(&worldMatrix));

        const float4& baseColor = pspRuntimeMaterial->GetBaseColor();
        const bool useLighting = UsesDirectionalLighting(pspRuntimeMaterial);
        PspRuntimeTexture* texture = nullptr;
        const bool hasTexture = pspRuntimeMaterial->TryResolveTexture(texture);
        if (LightingSettings.Pipeline == PspLightingPipeline::FixedFunctionLambert) {
            if (hasTexture) {
                SubmitFixedFunctionTexturedDrawable(record, baseColor, useLighting, CurrentLighting.HasDirectionalLight, texture);
                return;
            }

            SubmitFixedFunctionDrawable(record, baseColor, useLighting, CurrentLighting.HasDirectionalLight);
            return;
        }

        if (hasTexture && record.TexCoords.empty()) {
            throw std::runtime_error("Textured PSP drawables require mesh texcoords.");
        }

        BindTexture(texture);

        if (hasTexture) {
            int32_t vertexCount = static_cast<int32_t>(record.Indices.empty() ? record.Positions.size() : record.Indices.size());
            if (vertexCount < 3) {
                return;
            }

            PspTexturedLitVertex* vertices = static_cast<PspTexturedLitVertex*>(sceGuGetMemory(sizeof(PspTexturedLitVertex) * static_cast<std::size_t>(vertexCount)));
            for (int32_t index = 0; index < vertexCount; index++) {
                std::uint32_t sourceIndex = record.Indices.empty()
                    ? static_cast<std::uint32_t>(index)
                    : record.Indices[static_cast<std::size_t>(index)];
                if (sourceIndex >= record.Positions.size() || sourceIndex >= record.TexCoords.size()) {
                    throw std::runtime_error("PSP textured draw submitted an out-of-range mesh index.");
                }

                const float3& position = record.Positions[sourceIndex];
                const float3 sourceNormal = sourceIndex < record.Normals.size()
                    ? record.Normals[sourceIndex]
                    : float3(0.0f, 0.0f, 1.0f);
                const float3 worldNormal = float3::Normalize(RotateNormal(sourceNormal, drawable->get_Parent()));
                const float2& texCoord = record.TexCoords[sourceIndex];
                const float4 litColor = EvaluateCpuLitColor(baseColor, worldNormal, useLighting, LightingSettings, CurrentLighting);

                vertices[index] = PspTexturedLitVertex {
                    texCoord.X,
                    texCoord.Y,
                    ConvertColorToAbgr(litColor),
                    position.X,
                    position.Y,
                    position.Z
                };
            }

            sceGumDrawArray(
                GU_TRIANGLES,
                GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                vertexCount,
                nullptr,
                vertices);
            return;
        }

        SubmitCpuLitDrawable(drawable, record, baseColor, useLighting, LightingSettings, CurrentLighting);
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
                const float3 lightForward = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), directionalLight->get_Parent()->get_Orientation());
                CurrentLighting.DirectionalLightDirection = float3(-lightForward.X, -lightForward.Y, -lightForward.Z);
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
            ResolveSceneLighting();
            if (LightingSettings.Pipeline == PspLightingPipeline::FixedFunctionLambert) {
                ConfigureFixedFunctionSceneLighting(LightingSettings, CurrentLighting);
            } else {
                sceGuDisable(GU_LIGHT0);
                sceGuDisable(GU_LIGHTING);
            }
            renderQueue->VisitOrdered(this);
        }
    }
}
