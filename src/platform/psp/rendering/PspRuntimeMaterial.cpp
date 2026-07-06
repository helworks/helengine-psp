#include "platform/psp/rendering/PspRuntimeMaterial.hpp"

#include <cstring>
#include <stdexcept>

#include "MaterialConstantBufferAsset.hpp"

namespace helengine::psp::rendering {
    namespace {
        /// Constant-buffer name used for the authored base color payload.
        constexpr const char* BaseColorBufferName = "BaseColorBuffer";

        /// Constant-buffer name used for the PSP lighting configuration payload.
        constexpr const char* LightingConfigBufferName = "LightingConfigBuffer";
    }

    /// Creates one PSP runtime material with lit-directional defaults.
    PspRuntimeMaterial::PspRuntimeMaterial()
        : BaseColor(1.0f, 1.0f, 1.0f, 1.0f),
          HasAuthoredBaseColor(false),
          ReceivesLighting(true),
          HasAuthoredLightingConfiguration(false),
          LightingResponse(PspMaterialLightingResponse::LitDirectional) {
    }

    /// Gets the authored base color used by the PSP renderer.
    const float4& PspRuntimeMaterial::GetBaseColor() const {
        if (HasAuthoredBaseColor) {
            return BaseColor;
        }

        const PspRuntimeMaterial* parentMaterial = GetParentPspRuntimeMaterial();
        if (parentMaterial != nullptr) {
            return parentMaterial->GetBaseColor();
        }

        return BaseColor;
    }

    /// Gets whether the material receives scene lighting.
    bool PspRuntimeMaterial::GetReceivesLighting() const {
        if (HasAuthoredLightingConfiguration) {
            return ReceivesLighting;
        }

        const PspRuntimeMaterial* parentMaterial = GetParentPspRuntimeMaterial();
        if (parentMaterial != nullptr) {
            return parentMaterial->GetReceivesLighting();
        }

        return ReceivesLighting;
    }

    /// Gets the PSP lighting-response mode.
    PspMaterialLightingResponse PspRuntimeMaterial::GetLightingResponse() const {
        if (HasAuthoredLightingConfiguration) {
            return LightingResponse;
        }

        const PspRuntimeMaterial* parentMaterial = GetParentPspRuntimeMaterial();
        if (parentMaterial != nullptr) {
            return parentMaterial->GetLightingResponse();
        }

        return LightingResponse;
    }

    /// Resolves the first bound PSP runtime texture when the material exposes one.
    bool PspRuntimeMaterial::TryResolveTexture(PspRuntimeTexture*& texture) {
        RuntimeTexture* resolvedTexture = ResolveTexture();
        if (resolvedTexture == nullptr) {
            texture = nullptr;
            return false;
        }

        texture = dynamic_cast<PspRuntimeTexture*>(resolvedTexture);
        if (texture == nullptr) {
            throw std::runtime_error("PSP textured materials require PspRuntimeTexture instances.");
        }

        return true;
    }

    /// Loads PSP material state from one cooked material asset.
    void PspRuntimeMaterial::LoadFromCooked(ShaderMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw std::invalid_argument("PSP cooked material data is required.");
        }

        this->set_Id(materialAsset->get_Id());
        BaseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
        HasAuthoredBaseColor = false;
        ReceivesLighting = true;
        HasAuthoredLightingConfiguration = false;
        LightingResponse = PspMaterialLightingResponse::LitDirectional;
        if (materialAsset->ConstantBuffers == nullptr) {
            return;
        }

        for (int32_t index = 0; index < materialAsset->ConstantBuffers->Length; index++) {
            MaterialConstantBufferAsset* constantBuffer = (*materialAsset->ConstantBuffers)[index];
            if (constantBuffer == nullptr || constantBuffer->Data == nullptr) {
                continue;
            }

            if (constantBuffer->get_Name() == BaseColorBufferName && constantBuffer->Data->Length >= 16) {
                float components[4] {};
                std::memcpy(components, constantBuffer->Data->Data, sizeof(components));
                BaseColor = float4(components[0], components[1], components[2], components[3]);
                HasAuthoredBaseColor = true;
            } else if (constantBuffer->get_Name() == LightingConfigBufferName && constantBuffer->Data->Length >= 8) {
                float configuration[2] {};
                std::memcpy(configuration, constantBuffer->Data->Data, sizeof(configuration));
                ReceivesLighting = configuration[0] >= 0.5f;
                LightingResponse = configuration[1] < 0.5f
                    ? PspMaterialLightingResponse::Unlit
                    : PspMaterialLightingResponse::LitDirectional;
                HasAuthoredLightingConfiguration = true;
            }
        }
    }

    /// Resolves the parent PSP runtime material when this material inherits PSP-specific authored state.
    const PspRuntimeMaterial* PspRuntimeMaterial::GetParentPspRuntimeMaterial() const {
        RuntimeMaterial* parentMaterial = const_cast<PspRuntimeMaterial*>(this)->get_ParentMaterial();
        if (parentMaterial == nullptr) {
            return nullptr;
        }

        const PspRuntimeMaterial* parentPspMaterial = dynamic_cast<PspRuntimeMaterial*>(parentMaterial);
        if (parentPspMaterial == nullptr) {
            throw std::runtime_error("PSP runtime materials must inherit from other PSP runtime materials.");
        }

        return parentPspMaterial;
    }
}
