#include "platform/psp/rendering/PspRuntimeMaterial.hpp"

#include <stdexcept>

namespace helengine::psp::rendering {
    /// Creates one PSP runtime material with lit-directional defaults.
    PspRuntimeMaterial::PspRuntimeMaterial()
        : BaseColor(1.0f, 1.0f, 1.0f, 1.0f),
          HasAuthoredBaseColor(false),
          ReceivesLighting(true),
          HasAuthoredLightingConfiguration(false),
          LightingResponse(PspMaterialLightingResponse::LitDirectional),
          OwnedTexture(nullptr) {
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
        RuntimeTexture* resolvedTexture = ResolvePrimaryTexture();
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

    /// Assigns the texture loaded specifically for this cooked PSP material and exposes it to the fixed-function renderer.
    void PspRuntimeMaterial::SetPrimaryTexture(RuntimeTexture* texture) {
        OwnedTexture = texture;
        RuntimeMaterial::SetPrimaryTexture(texture);
    }

    /// Gets the texture owned by this material's cooked texture reference, when one was authored.
    RuntimeTexture* PspRuntimeMaterial::GetOwnedTexture() const {
        return OwnedTexture;
    }

    /// Loads PSP material state from one cooked material asset.
    void PspRuntimeMaterial::LoadFromCooked(PlatformMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw std::invalid_argument("PSP cooked material data is required.");
        }

        this->set_Id(materialAsset->get_Id());
        BaseColor = float4(
            static_cast<float>(materialAsset->BaseColorR) / 255.0f,
            static_cast<float>(materialAsset->BaseColorG) / 255.0f,
            static_cast<float>(materialAsset->BaseColorB) / 255.0f,
            static_cast<float>(materialAsset->BaseColorA) / 255.0f);
        HasAuthoredBaseColor = true;
        ReceivesLighting = materialAsset->Lit;
        HasAuthoredLightingConfiguration = true;
        LightingResponse = ReceivesLighting
            ? PspMaterialLightingResponse::LitDirectional
            : PspMaterialLightingResponse::Unlit;
        this->set_CastsShadows(ReceivesLighting);
        this->set_ReceivesShadows(ReceivesLighting);
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
