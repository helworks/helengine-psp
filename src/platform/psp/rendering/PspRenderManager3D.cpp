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
#include "platform/psp/PspBootTrace.hpp"
#include "platform/psp/rendering/PspRuntimeMaterial.hpp"
#include "platform/psp/rendering/PspRuntimeModel.hpp"
#include "platform/psp/rendering/PspRuntimeTexture.hpp"

namespace helengine::psp::rendering {
    namespace {
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

        /// Stores one raw 4x4 float buffer that can be reinterpreted as a PSP GU matrix.
        struct alignas(16) PspMatrixBuffer {
            float M[4][4];
        };

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

#if defined(HELENGINE_PSP_ENABLE_BOOT_TRACE) && HELENGINE_PSP_ENABLE_BOOT_TRACE
        /// Returns whether one world-space value is within a small tolerance of the expected authored showcase value.
        bool IsApproximately(float value, float expectedValue, float tolerance) {
            return std::fabs(value - expectedValue) <= tolerance;
        }

        /// Returns whether one drawable transform matches the authored central plaza tower used for lighting diagnostics.
        bool IsDirectionalShadowCentralTower(Entity* entity) {
            if (entity == nullptr) {
                return false;
            }

            const float3 position = entity->get_Position();
            const float3 scale = entity->get_Scale();
            return IsApproximately(position.X, 0.0f, 0.01f)
                && IsApproximately(position.Y, 9.0f, 0.01f)
                && IsApproximately(position.Z, -12.0f, 0.01f)
                && IsApproximately(scale.X, 7.0f, 0.01f)
                && IsApproximately(scale.Y, 18.0f, 0.01f)
                && IsApproximately(scale.Z, 7.0f, 0.01f);
        }

        /// Periodically writes the camera-facing and light-facing dot values for the authored plaza tower faces.
        void WritePlazaTowerFaceDebugTrace(Entity* entity, const float3& cameraPosition, const float3& lightDirection) {
            if (!IsDirectionalShadowCentralTower(entity)) {
                return;
            }

            static int32_t PlazaTowerTraceCounter = 0;
            PlazaTowerTraceCounter++;
            if ((PlazaTowerTraceCounter % 120) != 0) {
                return;
            }

            const float3 entityPosition = entity->get_Position();
            const float4 entityOrientation = entity->get_Orientation();
            const float3 cameraToTower = float3::Normalize(cameraPosition - entityPosition);
            const float3 normalizedLightDirection = float3::Normalize(lightDirection);

            const float3 positiveX = float4::RotateVector(float3(1.0f, 0.0f, 0.0f), entityOrientation);
            const float3 negativeX = float4::RotateVector(float3(-1.0f, 0.0f, 0.0f), entityOrientation);
            const float3 positiveZ = float4::RotateVector(float3(0.0f, 0.0f, 1.0f), entityOrientation);
            const float3 negativeZ = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), entityOrientation);

            PspBootTrace::WriteLine(
                std::string("PlazaTowerFaceDebug cameraToTower=")
                + std::to_string(cameraToTower.X) + ","
                + std::to_string(cameraToTower.Y) + ","
                + std::to_string(cameraToTower.Z)
                + " light=" + std::to_string(normalizedLightDirection.X) + ","
                + std::to_string(normalizedLightDirection.Y) + ","
                + std::to_string(normalizedLightDirection.Z)
                + " pxView=" + std::to_string(float3::Dot(positiveX, cameraToTower))
                + " pxLight=" + std::to_string(float3::Dot(positiveX, normalizedLightDirection))
                + " nxView=" + std::to_string(float3::Dot(negativeX, cameraToTower))
                + " nxLight=" + std::to_string(float3::Dot(negativeX, normalizedLightDirection))
                + " pzView=" + std::to_string(float3::Dot(positiveZ, cameraToTower))
                + " pzLight=" + std::to_string(float3::Dot(positiveZ, normalizedLightDirection))
                + " nzView=" + std::to_string(float3::Dot(negativeZ, cameraToTower))
                + " nzLight=" + std::to_string(float3::Dot(negativeZ, normalizedLightDirection)));
        }
#endif

        /// Returns the default local-space normal used when authored mesh data omits normals.
        float3 GetFallbackNormal() {
            return float3(0.0f, 0.0f, 1.0f);
        }

        /// Returns the number of vertices submitted after expanding the authored index stream.
        std::size_t GetExpandedVertexCount(ModelAsset* data) {
            if (data == nullptr || data->Positions == nullptr) {
                return 0;
            }

            if (data->Indices32 != nullptr && data->Indices32->Length > 0) {
                return static_cast<std::size_t>(data->Indices32->Length);
            }

            if (data->Indices16 != nullptr && data->Indices16->Length > 0) {
                return static_cast<std::size_t>(data->Indices16->Length);
            }

            return static_cast<std::size_t>(data->Positions->Length);
        }

        /// Resolves one authored source vertex index from the packed mesh data.
        std::uint32_t ResolveExpandedSourceIndex(ModelAsset* data, int32_t expandedVertexIndex) {
            if (data == nullptr || data->Positions == nullptr || data->Positions->Length <= 0) {
                throw std::runtime_error("PSP runtime-model generation requires authored mesh positions.");
            }

            if (data->Indices32 != nullptr && data->Indices32->Length > 0) {
                if (expandedVertexIndex < 0 || expandedVertexIndex >= data->Indices32->Length) {
                    throw std::runtime_error("PSP runtime-model generation submitted an out-of-range 32-bit index lookup.");
                }

                return (*data->Indices32)[expandedVertexIndex];
            }

            if (data->Indices16 != nullptr && data->Indices16->Length > 0) {
                if (expandedVertexIndex < 0 || expandedVertexIndex >= data->Indices16->Length) {
                    throw std::runtime_error("PSP runtime-model generation submitted an out-of-range 16-bit index lookup.");
                }

                return (*data->Indices16)[expandedVertexIndex];
            }

            return static_cast<std::uint32_t>(expandedVertexIndex);
        }

        /// Builds the ready-to-submit untextured fixed-function PSP vertex stream for one authored mesh.
        std::vector<PspRuntimeModel::FixedFunctionVertex> BuildFixedFunctionVertices(ModelAsset* data) {
            std::size_t vertexCount = GetExpandedVertexCount(data);
            std::vector<PspRuntimeModel::FixedFunctionVertex> vertices;
            if (vertexCount == 0) {
                return vertices;
            }

            vertices.reserve(vertexCount);
            for (int32_t expandedVertexIndex = 0; expandedVertexIndex < static_cast<int32_t>(vertexCount); expandedVertexIndex++) {
                std::uint32_t sourceIndex = ResolveExpandedSourceIndex(data, expandedVertexIndex);
                if (sourceIndex >= static_cast<std::uint32_t>(data->Positions->Length)) {
                    throw std::runtime_error("PSP runtime-model generation submitted an out-of-range mesh position index.");
                }

                const float3& position = (*data->Positions)[sourceIndex];
                const float3 sourceNormal = data->Normals != nullptr && sourceIndex < static_cast<std::uint32_t>(data->Normals->Length)
                    ? (*data->Normals)[sourceIndex]
                    : GetFallbackNormal();

                vertices.push_back(PspRuntimeModel::FixedFunctionVertex {
                    sourceNormal.X,
                    sourceNormal.Y,
                    sourceNormal.Z,
                    position.X,
                    position.Y,
                    position.Z
                });
            }

            return vertices;
        }

        /// Builds the ready-to-submit textured fixed-function PSP vertex stream for one authored mesh.
        std::vector<PspRuntimeModel::FixedFunctionTexturedVertex> BuildFixedFunctionTexturedVertices(ModelAsset* data) {
            std::vector<PspRuntimeModel::FixedFunctionTexturedVertex> vertices;
            if (data == nullptr || data->TexCoords == nullptr || data->TexCoords->Length <= 0) {
                return vertices;
            }

            std::size_t vertexCount = GetExpandedVertexCount(data);
            if (vertexCount == 0) {
                return vertices;
            }

            vertices.reserve(vertexCount);
            for (int32_t expandedVertexIndex = 0; expandedVertexIndex < static_cast<int32_t>(vertexCount); expandedVertexIndex++) {
                std::uint32_t sourceIndex = ResolveExpandedSourceIndex(data, expandedVertexIndex);
                if (sourceIndex >= static_cast<std::uint32_t>(data->Positions->Length)) {
                    throw std::runtime_error("PSP runtime-model generation submitted an out-of-range mesh position index.");
                } else if (sourceIndex >= static_cast<std::uint32_t>(data->TexCoords->Length)) {
                    throw std::runtime_error("PSP runtime-model generation submitted an out-of-range mesh texcoord index.");
                }

                const float3& position = (*data->Positions)[sourceIndex];
                const float3 sourceNormal = data->Normals != nullptr && sourceIndex < static_cast<std::uint32_t>(data->Normals->Length)
                    ? (*data->Normals)[sourceIndex]
                    : GetFallbackNormal();
                const float2& texCoord = (*data->TexCoords)[sourceIndex];

                vertices.push_back(PspRuntimeModel::FixedFunctionTexturedVertex {
                    texCoord.X,
                    texCoord.Y,
                    sourceNormal.X,
                    sourceNormal.Y,
                    sourceNormal.Z,
                    position.X,
                    position.Y,
                    position.Z
                });
            }

            return vertices;
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

        /// Builds one world matrix that preserves only rotation and translation for GPU-lit scaled draws.
        float4x4 BuildWorldMatrixWithoutScale(Entity* entity) {
            if (entity == nullptr) {
                return float4x4::get_Identity();
            }

            float4 orientation = entity->get_Orientation();
            float3 position = entity->get_Position();

            float4x4 rotation;
            float4x4::CreateFromQuaternion(orientation, rotation);

            float4x4 translation;
            float4x4::CreateTranslation(position, translation);

            float4x4 world;
            float4x4::Multiply(rotation, translation, world);
            return world;
        }

        /// Returns whether one world scale uses distinct axis magnitudes that would skew fixed-function normals.
        bool HasNonUniformScale(const float3& scale) {
            constexpr float UniformScaleTolerance = 0.0001f;
            return std::fabs(scale.X - scale.Y) > UniformScaleTolerance
                || std::fabs(scale.X - scale.Z) > UniformScaleTolerance
                || std::fabs(scale.Y - scale.Z) > UniformScaleTolerance;
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

        /// Builds one transient fixed-function vertex stream with object-space positions pre-scaled for non-uniform GPU lighting.
        PspRuntimeModel::FixedFunctionVertex* CreateScaledFixedFunctionVertices(
            const PspRuntimeModel* runtimeModel,
            const float3& scale) {
            if (runtimeModel == nullptr || !runtimeModel->HasFixedFunctionVertices()) {
                return nullptr;
            }

            int32_t vertexCount = runtimeModel->GetFixedFunctionVertexCount();
            if (vertexCount < 1) {
                return nullptr;
            }

            const PspRuntimeModel::FixedFunctionVertex* sourceVertices = runtimeModel->GetFixedFunctionVertices();
            PspRuntimeModel::FixedFunctionVertex* vertices = static_cast<PspRuntimeModel::FixedFunctionVertex*>(
                sceGuGetMemory(sizeof(PspRuntimeModel::FixedFunctionVertex) * static_cast<std::size_t>(vertexCount)));
            for (int32_t index = 0; index < vertexCount; index++) {
                const PspRuntimeModel::FixedFunctionVertex& sourceVertex = sourceVertices[index];
                vertices[index] = PspRuntimeModel::FixedFunctionVertex {
                    sourceVertex.NX,
                    sourceVertex.NY,
                    sourceVertex.NZ,
                    sourceVertex.X * scale.X,
                    sourceVertex.Y * scale.Y,
                    sourceVertex.Z * scale.Z
                };
            }

            return vertices;
        }

        /// Builds one transient textured fixed-function vertex stream with object-space positions pre-scaled for non-uniform GPU lighting.
        PspRuntimeModel::FixedFunctionTexturedVertex* CreateScaledFixedFunctionTexturedVertices(
            const PspRuntimeModel* runtimeModel,
            const float3& scale) {
            if (runtimeModel == nullptr || !runtimeModel->HasFixedFunctionTexturedVertices()) {
                return nullptr;
            }

            int32_t vertexCount = runtimeModel->GetFixedFunctionTexturedVertexCount();
            if (vertexCount < 1) {
                return nullptr;
            }

            const PspRuntimeModel::FixedFunctionTexturedVertex* sourceVertices = runtimeModel->GetFixedFunctionTexturedVertices();
            PspRuntimeModel::FixedFunctionTexturedVertex* vertices = static_cast<PspRuntimeModel::FixedFunctionTexturedVertex*>(
                sceGuGetMemory(sizeof(PspRuntimeModel::FixedFunctionTexturedVertex) * static_cast<std::size_t>(vertexCount)));
            for (int32_t index = 0; index < vertexCount; index++) {
                const PspRuntimeModel::FixedFunctionTexturedVertex& sourceVertex = sourceVertices[index];
                vertices[index] = PspRuntimeModel::FixedFunctionTexturedVertex {
                    sourceVertex.U,
                    sourceVertex.V,
                    sourceVertex.NX,
                    sourceVertex.NY,
                    sourceVertex.NZ,
                    sourceVertex.X * scale.X,
                    sourceVertex.Y * scale.Y,
                    sourceVertex.Z * scale.Z
                };
            }

            return vertices;
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

        /// Submits one drawable through the existing CPU-lit vertex path.
        void SubmitCpuLitDrawable(
            IDrawable3D* drawable,
            const PspRuntimeModel* runtimeModel,
            const float4& baseColor,
            bool useLighting,
            const PspLightingSettings& lightingSettings,
            const PspSceneLightingSnapshot& lightingSnapshot) {
            if (runtimeModel == nullptr || !runtimeModel->HasFixedFunctionVertices()) {
                return;
            }

            int32_t vertexCount = runtimeModel->GetFixedFunctionVertexCount();
            if (vertexCount < 3) {
                return;
            }

            PspLitVertex* vertices = static_cast<PspLitVertex*>(sceGuGetMemory(sizeof(PspLitVertex) * static_cast<std::size_t>(vertexCount)));
            const PspRuntimeModel::FixedFunctionVertex* sourceVertices = runtimeModel->GetFixedFunctionVertices();
            for (int32_t index = 0; index < vertexCount; index++) {
                const PspRuntimeModel::FixedFunctionVertex& sourceVertex = sourceVertices[index];
                const float3 position(sourceVertex.X, sourceVertex.Y, sourceVertex.Z);
                const float3 sourceNormal(sourceVertex.NX, sourceVertex.NY, sourceVertex.NZ);
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

    }

    /// Creates the PSP 3D render manager.
    PspRenderManager3D::PspRenderManager3D()
        : CurrentView(float4x4::get_Identity()),
          CurrentProjection(float4x4::get_Identity()),
          CurrentCameraPosition(0.0f, 0.0f, 0.0f),
          RenderManager2D(nullptr),
          HasCachedTextureEnabledState(false),
          CachedTextureEnabledState(false),
          CachedTexture(nullptr),
          HasCachedLightingEnabledState(false),
          CachedLightingEnabledState(false),
          HasCachedLight0EnabledState(false),
          CachedLight0EnabledState(false) {
    }

    /// Resets the renderer-owned GU state cache before one camera pass begins.
    void PspRenderManager3D::ResetCachedGuState() {
        HasCachedTextureEnabledState = false;
        CachedTextureEnabledState = false;
        CachedTexture = nullptr;
        HasCachedLightingEnabledState = false;
        CachedLightingEnabledState = false;
        HasCachedLight0EnabledState = false;
        CachedLight0EnabledState = false;
    }

    /// Applies the requested PSP texturing state only when it differs from the active GU cache.
    void PspRenderManager3D::SetTextureEnabled(bool enabled) {
        if (HasCachedTextureEnabledState && CachedTextureEnabledState == enabled) {
            return;
        }

        HasCachedTextureEnabledState = true;
        CachedTextureEnabledState = enabled;
        if (enabled) {
            sceGuEnable(GU_TEXTURE_2D);
            return;
        }

        sceGuDisable(GU_TEXTURE_2D);
        CachedTexture = nullptr;
    }

    /// Applies the requested PSP lighting state only when it differs from the active GU cache.
    void PspRenderManager3D::SetLightingEnabled(bool enabled) {
        if (HasCachedLightingEnabledState && CachedLightingEnabledState == enabled) {
            return;
        }

        HasCachedLightingEnabledState = true;
        CachedLightingEnabledState = enabled;
        if (enabled) {
            sceGuEnable(GU_LIGHTING);
            return;
        }

        sceGuDisable(GU_LIGHTING);
    }

    /// Applies the requested PSP directional-light state only when it differs from the active GU cache.
    void PspRenderManager3D::SetLight0Enabled(bool enabled) {
        if (HasCachedLight0EnabledState && CachedLight0EnabledState == enabled) {
            return;
        }

        HasCachedLight0EnabledState = true;
        CachedLight0EnabledState = enabled;
        if (enabled) {
            sceGuEnable(GU_LIGHT0);
            return;
        }

        sceGuDisable(GU_LIGHT0);
    }

    /// Binds one PSP runtime texture for GU sampling or disables texturing when no texture exists.
    void PspRenderManager3D::BindTexture(PspRuntimeTexture* texture) {
        const std::uint64_t bindStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        std::uint64_t flushMicroseconds = 0;
        std::size_t byteCount = 0;
        if (texture == nullptr || !texture->HasPixels()) {
            SetTextureEnabled(false);
            PspRenderProfiler::Record3DTextureBind(
                texture,
                byteCount,
                PspRenderProfiler::GetTimestampMicroseconds() - bindStartMicroseconds,
                flushMicroseconds);
            return;
        }

        byteCount = texture->GetPixelCount() * sizeof(std::uint32_t);
        const bool needsTextureUpload = !HasCachedTextureEnabledState
            || !CachedTextureEnabledState
            || CachedTexture != texture;
        if (needsTextureUpload) {
            const std::uint64_t flushStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
            sceKernelDcacheWritebackRange(
                const_cast<std::uint32_t*>(texture->GetPixelsAbgr8888()),
                static_cast<unsigned int>(byteCount));
            flushMicroseconds = PspRenderProfiler::GetTimestampMicroseconds() - flushStartMicroseconds;

            SetTextureEnabled(true);
            sceGuTexMode(GU_PSM_8888, 0, 0, 0);
            sceGuTexImage(0, texture->get_Width(), texture->get_Height(), texture->get_Width(), texture->GetPixelsAbgr8888());
            sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
            sceGuTexFilter(GU_NEAREST, GU_NEAREST);
            sceGuTexWrap(GU_REPEAT, GU_REPEAT);
            CachedTexture = texture;
        }

        PspRenderProfiler::Record3DTextureBind(
            texture,
            byteCount,
            PspRenderProfiler::GetTimestampMicroseconds() - bindStartMicroseconds,
            flushMicroseconds);
    }

    /// Configures the scene-wide fixed-function lighting state for the active camera pass.
    void PspRenderManager3D::ConfigureFixedFunctionSceneLighting() {
        SetTextureEnabled(false);
        sceGuLightMode(GU_SINGLE_COLOR);
        sceGuSpecular(0.0f);
        sceGuAmbient(ConvertColorToAbgr(float4(
            LightingSettings.AmbientIntensity,
            LightingSettings.AmbientIntensity,
            LightingSettings.AmbientIntensity,
            1.0f)));

        if (!CurrentLighting.HasDirectionalLight) {
            SetLight0Enabled(false);
            return;
        }

        const float3 normalizedDirection = float3::Normalize(CurrentLighting.DirectionalLightDirection);
        const float3 scaledDirectionalColor = float3(
            CurrentLighting.DirectionalLightColor.X * CurrentLighting.DirectionalLightIntensity,
            CurrentLighting.DirectionalLightColor.Y * CurrentLighting.DirectionalLightIntensity,
            CurrentLighting.DirectionalLightColor.Z * CurrentLighting.DirectionalLightIntensity);
        const ScePspFVector3 lightVector = CreatePspVector3(normalizedDirection);

        SetLight0Enabled(true);
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
    void PspRenderManager3D::ConfigureFixedFunctionMaterial(const float4& baseColor, bool useLighting) {
        const std::uint32_t baseColorAbgr = ConvertColorToAbgr(baseColor);

        sceGuColor(baseColorAbgr);
        sceGuAmbientColor(baseColorAbgr);
        sceGuModelColor(0, baseColorAbgr, baseColorAbgr, 0);

        if (!useLighting) {
            SetLight0Enabled(false);
            SetLightingEnabled(false);
            return;
        }

        SetLightingEnabled(true);
        SetLight0Enabled(CurrentLighting.HasDirectionalLight);
    }

    /// Submits one drawable through the current fixed-function untextured lighting path.
    void PspRenderManager3D::SubmitFixedFunctionDrawable(
        const PspRuntimeModel* runtimeModel,
        const float4& baseColor,
        bool useLighting,
        const float3* positionScale) {
        if (runtimeModel == nullptr || !runtimeModel->HasFixedFunctionVertices()) {
            return;
        }

        int32_t vertexCount = runtimeModel->GetFixedFunctionVertexCount();
        if (vertexCount < 3) {
            return;
        }

        BindTexture(nullptr);
        const std::uint64_t materialSetupStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        ConfigureFixedFunctionMaterial(baseColor, useLighting);
        PspRenderProfiler::Record3DFixedFunctionMaterialSetup(PspRenderProfiler::GetTimestampMicroseconds() - materialSetupStartMicroseconds);

        const PspRuntimeModel::FixedFunctionVertex* vertices = positionScale != nullptr
            ? CreateScaledFixedFunctionVertices(runtimeModel, *positionScale)
            : runtimeModel->GetFixedFunctionVertices();
        const std::uint64_t drawStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        sceGumDrawArray(
            GU_TRIANGLES,
            GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
            vertexCount,
            nullptr,
            vertices);
        PspRenderProfiler::Record3DFixedFunctionDraw(PspRenderProfiler::GetTimestampMicroseconds() - drawStartMicroseconds);
    }

    /// Submits one drawable through the current fixed-function textured lighting path.
    void PspRenderManager3D::SubmitFixedFunctionTexturedDrawable(
        const PspRuntimeModel* runtimeModel,
        const float4& baseColor,
        bool useLighting,
        PspRuntimeTexture* texture,
        const float3* positionScale) {
        if (runtimeModel == nullptr || !runtimeModel->HasFixedFunctionTexturedVertices()) {
            return;
        }

        int32_t vertexCount = runtimeModel->GetFixedFunctionTexturedVertexCount();
        if (vertexCount < 3) {
            return;
        } else if (texture == nullptr || !texture->HasPixels()) {
            throw std::runtime_error("PSP fixed-function textured draws require a valid runtime texture.");
        }

        BindTexture(texture);
        const std::uint64_t materialSetupStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        ConfigureFixedFunctionMaterial(baseColor, useLighting);
        PspRenderProfiler::Record3DFixedFunctionMaterialSetup(PspRenderProfiler::GetTimestampMicroseconds() - materialSetupStartMicroseconds);

        const PspRuntimeModel::FixedFunctionTexturedVertex* vertices = positionScale != nullptr
            ? CreateScaledFixedFunctionTexturedVertices(runtimeModel, *positionScale)
            : runtimeModel->GetFixedFunctionTexturedVertices();
        const std::uint64_t drawStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        sceGumDrawArray(
            GU_TRIANGLES,
            GU_TEXTURE_32BITF | GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
            vertexCount,
            nullptr,
            vertices);
        PspRenderProfiler::Record3DFixedFunctionDraw(PspRenderProfiler::GetTimestampMicroseconds() - drawStartMicroseconds);
    }

    /// Builds a CPU-side runtime model payload from the raw mesh asset.
    RuntimeModel* PspRenderManager3D::BuildModelFromRaw(ModelAsset* data) {
        if (data != nullptr && !data->get_Id().empty()) {
            auto cachedModelIterator = CachedModels.find(data->get_Id());
            if (cachedModelIterator != CachedModels.end()) {
                PspBootTrace::WriteLine(
                    std::string("PspRuntimeModelReuse id=") +
                    data->get_Id() +
                    " ptr=" +
                    std::to_string(reinterpret_cast<std::uintptr_t>(cachedModelIterator->second)) +
                    " cacheSize=" +
                    std::to_string(CachedModels.size()));
                return cachedModelIterator->second;
            }
        }

        PspRuntimeModel* runtimeModel = new PspRuntimeModel();
        if (data != nullptr) {
            runtimeModel->set_Id(data->get_Id());
            runtimeModel->SetFixedFunctionVertices(BuildFixedFunctionVertices(data));
            runtimeModel->SetFixedFunctionTexturedVertices(BuildFixedFunctionTexturedVertices(data));
        }

        if (data != nullptr && !data->get_Id().empty()) {
            CachedModels.emplace(data->get_Id(), runtimeModel);
        }

        PspBootTrace::WriteLine(
            std::string("PspRuntimeModelBuild id=") +
            (data != nullptr ? data->get_Id() : std::string()) +
            " ptr=" +
            std::to_string(reinterpret_cast<std::uintptr_t>(runtimeModel)) +
            " cacheSize=" +
            std::to_string(CachedModels.size()));
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

        PspBootTrace::WriteLine(
            std::string("PspRuntimeMaterialBuild id=") +
            runtimeMaterial->get_Id() +
            " ptr=" +
            std::to_string(reinterpret_cast<std::uintptr_t>(runtimeMaterial)));
        return runtimeMaterial;
    }

    /// Releases one PSP runtime model after the final scene reference is removed.
    void PspRenderManager3D::ReleaseModel(RuntimeModel* model) {
        if (model == nullptr) {
            throw std::invalid_argument("PSP runtime-model release requires one runtime model instance.");
        }

        if (!model->get_Id().empty()) {
            auto cachedModelIterator = CachedModels.find(model->get_Id());
            if (cachedModelIterator != CachedModels.end() && cachedModelIterator->second == model) {
                CachedModels.erase(cachedModelIterator);
            }
        }

        PspBootTrace::WriteLine(
            std::string("PspRuntimeModelRelease id=") +
            model->get_Id() +
            " ptr=" +
            std::to_string(reinterpret_cast<std::uintptr_t>(model)) +
            " cacheSize=" +
            std::to_string(CachedModels.size()));
    }

    /// Releases one PSP runtime material after the final scene reference is removed.
    void PspRenderManager3D::ReleaseMaterial(RuntimeMaterial* material) {
        if (material == nullptr) {
            throw std::invalid_argument("PSP runtime-material release requires one runtime material instance.");
        }

        PspBootTrace::WriteLine(
            std::string("PspRuntimeMaterialRelease id=") +
            material->get_Id() +
            " ptr=" +
            std::to_string(reinterpret_cast<std::uintptr_t>(material)));
    }

    /// Wires the paired PSP 2D renderer used for per-camera UI submission.
    void PspRenderManager3D::SetRenderManager2D(PspRenderManager2D* renderManager2D) {
        RenderManager2D = renderManager2D;
    }

    /// Draws every visible authored camera to the current PSP back buffer.
    void PspRenderManager3D::Draw() {
        PspRenderProfiler::BeginFrame();
        RenderManager3D::Draw();
        if (Core::get_Instance() == nullptr || Core::get_Instance()->get_ObjectManager() == nullptr) {
            PspRenderProfiler::EndFrame();
            return;
        }

        List<ICamera*>* cameras = Core::get_Instance()->get_ObjectManager()->get_Cameras();
        if (cameras == nullptr || cameras->Count() == 0) {
            PspRenderProfiler::EndFrame();
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
        PspRenderProfiler::EndFrame();
    }

    /// Draws one queued mesh for the active camera.
    void PspRenderManager3D::Visit(IDrawable3D* drawable) {
        const std::uint64_t visitStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        if (drawable == nullptr || drawable->get_Parent() == nullptr || !drawable->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        Entity* drawableParent = drawable->get_Parent();
        RuntimeModel* runtimeModel = drawable->get_Model();
        if (runtimeModel == nullptr) {
            return;
        }

        PspRuntimeModel* pspRuntimeModelData = static_cast<PspRuntimeModel*>(runtimeModel);
        if (!pspRuntimeModelData->HasFixedFunctionVertices()) {
            return;
        }

        RuntimeMaterial* runtimeMaterial = drawable->get_Material();
        const std::uint64_t materialResolveStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        RuntimeMaterial* rootMaterial = runtimeMaterial != nullptr ? runtimeMaterial->ResolveRootMaterial() : nullptr;
        PspRenderProfiler::Record3DMaterialResolve(PspRenderProfiler::GetTimestampMicroseconds() - materialResolveStartMicroseconds);
        if (rootMaterial == nullptr) {
            return;
        }
        
        PspRuntimeMaterial* pspRuntimeMaterial = static_cast<PspRuntimeMaterial*>(runtimeMaterial);
        PspRuntimeMaterial* rootPspRuntimeMaterial = static_cast<PspRuntimeMaterial*>(rootMaterial);
        const float4& baseColor = rootPspRuntimeMaterial->GetBaseColor();
        const bool useLighting = UsesDirectionalLighting(rootPspRuntimeMaterial);
        PspRuntimeTexture* texture = nullptr;
        const bool hasTexture = pspRuntimeMaterial->TryResolveTexture(texture);
        const float3 worldScale = drawableParent->get_Scale();
        const bool useScaledGpuVertices = LightingSettings.Pipeline == PspLightingPipeline::FixedFunctionLambert
            && useLighting
            && HasNonUniformScale(worldScale);
#if defined(HELENGINE_PSP_ENABLE_BOOT_TRACE) && HELENGINE_PSP_ENABLE_BOOT_TRACE
        if (CurrentLighting.HasDirectionalLight && useLighting) {
            WritePlazaTowerFaceDebugTrace(drawableParent, CurrentCameraPosition, CurrentLighting.DirectionalLightDirection);
        }
#endif

        const std::uint64_t worldMatrixBuildStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        float4x4 world = useScaledGpuVertices
            ? BuildWorldMatrixWithoutScale(drawableParent)
            : BuildWorldMatrix(drawableParent);
        PspRenderProfiler::Record3DWorldMatrixBuild(PspRenderProfiler::GetTimestampMicroseconds() - worldMatrixBuildStartMicroseconds);
        PspMatrixBuffer worldMatrix = CreatePspMatrixBuffer(world);

        const std::uint64_t modelMatrixLoadStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadMatrix(reinterpret_cast<ScePspFMatrix4*>(&worldMatrix));
        PspRenderProfiler::Record3DModelMatrixLoad(PspRenderProfiler::GetTimestampMicroseconds() - modelMatrixLoadStartMicroseconds);
        if (LightingSettings.Pipeline == PspLightingPipeline::FixedFunctionLambert) {
            if (hasTexture) {
                SubmitFixedFunctionTexturedDrawable(
                    pspRuntimeModelData,
                    baseColor,
                    useLighting,
                    texture,
                    useScaledGpuVertices ? &worldScale : nullptr);
                PspRenderProfiler::Record3DVisit(PspRenderProfiler::GetTimestampMicroseconds() - visitStartMicroseconds);
                return;
            }

            SubmitFixedFunctionDrawable(
                pspRuntimeModelData,
                baseColor,
                useLighting,
                useScaledGpuVertices ? &worldScale : nullptr);
            PspRenderProfiler::Record3DVisit(PspRenderProfiler::GetTimestampMicroseconds() - visitStartMicroseconds);
            return;
        }

        if (hasTexture && !pspRuntimeModelData->HasFixedFunctionTexturedVertices()) {
            throw std::runtime_error("Textured PSP drawables require mesh texcoords.");
        }

        BindTexture(texture);

        if (hasTexture) {
            int32_t vertexCount = pspRuntimeModelData->GetFixedFunctionTexturedVertexCount();
            if (vertexCount < 3) {
                return;
            }

            PspTexturedLitVertex* vertices = static_cast<PspTexturedLitVertex*>(sceGuGetMemory(sizeof(PspTexturedLitVertex) * static_cast<std::size_t>(vertexCount)));
            const PspRuntimeModel::FixedFunctionTexturedVertex* sourceVertices = pspRuntimeModelData->GetFixedFunctionTexturedVertices();
            for (int32_t index = 0; index < vertexCount; index++) {
                const PspRuntimeModel::FixedFunctionTexturedVertex& sourceVertex = sourceVertices[index];
                const float3 position(sourceVertex.X, sourceVertex.Y, sourceVertex.Z);
                const float3 sourceNormal(sourceVertex.NX, sourceVertex.NY, sourceVertex.NZ);
                const float3 worldNormal = float3::Normalize(RotateNormal(sourceNormal, drawableParent));
                const float4 litColor = EvaluateCpuLitColor(baseColor, worldNormal, useLighting, LightingSettings, CurrentLighting);

                vertices[index] = PspTexturedLitVertex {
                    sourceVertex.U,
                    sourceVertex.V,
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
            PspRenderProfiler::Record3DVisit(PspRenderProfiler::GetTimestampMicroseconds() - visitStartMicroseconds);
            return;
        }

        SubmitCpuLitDrawable(drawable, pspRuntimeModelData, baseColor, useLighting, LightingSettings, CurrentLighting);
        PspRenderProfiler::Record3DVisit(PspRenderProfiler::GetTimestampMicroseconds() - visitStartMicroseconds);
    }

    /// Resolves the active scene lighting for the current render pass.
    void PspRenderManager3D::ResolveSceneLighting() {
        CurrentLighting = PspSceneLightingSnapshot();

        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr || core->get_ObjectManager()->get_DirectionalLights() == nullptr) {
            return;
        }

        List<DirectionalLightComponent*>* directionalLights = core->get_ObjectManager()->get_DirectionalLights();
        for (int32_t lightIndex = 0; lightIndex < directionalLights->Count(); lightIndex++) {
            DirectionalLightComponent* directionalLight = (*directionalLights)[lightIndex];
            if (directionalLight == nullptr || directionalLight->get_Parent() == nullptr || !directionalLight->get_Parent()->get_IsHierarchyEnabled()) {
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

    /// Renders the currently active 3D queue for one camera.
    void PspRenderManager3D::RenderCamera(ICamera* camera) {
        const std::uint64_t renderStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
        Entity* cameraParent = camera->get_Parent();
        float3 cameraPosition = cameraParent->get_Position();
        float4 cameraOrientation = cameraParent->get_Orientation();
        float3 cameraForward = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), cameraOrientation);
        float3 cameraUp = float4::RotateVector(float3(0.0f, 1.0f, 0.0f), cameraOrientation);
        float3 cameraTarget = cameraPosition + cameraForward;
        CurrentCameraPosition = cameraPosition;

        float4x4::CreateLookAt(cameraPosition, cameraTarget, cameraUp, CurrentView);

        float aspectRatio = 1.0f;
        const int2 mainWindowSize = get_MainWindowSize();
        if (mainWindowSize.Y > 0) {
            aspectRatio = static_cast<float>(mainWindowSize.X) / static_cast<float>(mainWindowSize.Y);
        }

        constexpr float CameraFieldOfViewRadians = 0.78539816339f;
        constexpr float NearPlaneDistance = 0.1f;
        constexpr float FarPlaneDistance = 100.0f;
        float4x4::CreatePerspectiveFieldOfView(CameraFieldOfViewRadians, aspectRatio, NearPlaneDistance, FarPlaneDistance, CurrentProjection);

        PspMatrixBuffer viewMatrix = CreatePspMatrixBuffer(CurrentView);
        PspMatrixBuffer projectionMatrix = CreatePspMatrixBuffer(CurrentProjection);

        ResetCachedGuState();
        SetTextureEnabled(false);
        sceGuEnable(GU_DEPTH_TEST);
        sceGuDisable(GU_CULL_FACE);

        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadMatrix(reinterpret_cast<ScePspFMatrix4*>(&projectionMatrix));
        sceGumMatrixMode(GU_VIEW);
        sceGumLoadMatrix(reinterpret_cast<ScePspFMatrix4*>(&viewMatrix));
        PspRenderProfiler::Record3DCameraSetup(PspRenderProfiler::GetTimestampMicroseconds() - renderStartMicroseconds);

        IRenderQueue3D* renderQueue = camera->get_RenderQueue3D();
        int32_t drawableCount = 0;
        if (renderQueue != nullptr) {
            drawableCount = renderQueue->get_Count();
            const std::uint64_t sceneLightResolveStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
            ResolveSceneLighting();
            PspRenderProfiler::Record3DSceneLightResolve(PspRenderProfiler::GetTimestampMicroseconds() - sceneLightResolveStartMicroseconds);
            if (LightingSettings.Pipeline == PspLightingPipeline::FixedFunctionLambert) {
                ConfigureFixedFunctionSceneLighting();
            } else {
                SetLight0Enabled(false);
                SetLightingEnabled(false);
            }
            const std::uint64_t queueVisitStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
            renderQueue->VisitOrdered(this);
            PspRenderProfiler::Record3DQueueVisit(PspRenderProfiler::GetTimestampMicroseconds() - queueVisitStartMicroseconds);
        }

        std::uint64_t uiMicroseconds = 0;
        if (RenderManager2D != nullptr) {
            const std::uint64_t uiStartMicroseconds = PspRenderProfiler::GetTimestampMicroseconds();
            RenderManager2D->RenderCamera(camera);
            uiMicroseconds = PspRenderProfiler::GetTimestampMicroseconds() - uiStartMicroseconds;
        }
        PspRenderProfiler::Record3DCamera(
            drawableCount,
            PspRenderProfiler::GetTimestampMicroseconds() - renderStartMicroseconds,
            uiMicroseconds);
    }
}
