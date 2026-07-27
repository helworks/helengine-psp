#pragma once

#include <cstdint>

#include "PlatformMaterialAsset.hpp"
#include "RuntimeMaterial.hpp"
#include "float4.hpp"
#include "platform/psp/rendering/PspMaterialLightingResponse.hpp"
#include "platform/psp/rendering/PspRuntimeTexture.hpp"

namespace helengine::psp::rendering {
    /// Stores PSP-specific runtime material state derived from the cooked material payload.
    class PspRuntimeMaterial final : public RuntimeMaterial {
    public:
        /// Creates one PSP runtime material with lit-directional defaults.
        PspRuntimeMaterial();

        /// Gets the authored base color used by the PSP renderer.
        const float4& GetBaseColor() const;

        /// Gets whether the material receives scene lighting.
        bool GetReceivesLighting() const;

        /// Gets whether the material should render both triangle winding directions.
        bool IsDoubleSided() const;

        /// Gets the PSP lighting-response mode.
        PspMaterialLightingResponse GetLightingResponse() const;

        /// Resolves the first bound PSP runtime texture when the material exposes one.
        bool TryResolveTexture(PspRuntimeTexture*& texture);

        /// Assigns the texture loaded specifically for this cooked PSP material and exposes it to the fixed-function renderer.
        void SetPrimaryTexture(RuntimeTexture* texture) override;

        /// Gets the texture owned by this material's cooked texture reference, when one was authored.
        RuntimeTexture* GetOwnedTexture() const;

        /// Loads PSP material state from one cooked material asset.
        void LoadFromCooked(PlatformMaterialAsset* materialAsset);

    private:
        /// Resolves the parent PSP runtime material when this material inherits PSP-specific authored state.
        const PspRuntimeMaterial* GetParentPspRuntimeMaterial() const;

        /// Stores the authored base color.
        float4 BaseColor;

        /// Stores whether one cooked base-color buffer explicitly authored the local base color.
        bool HasAuthoredBaseColor;

        /// Stores whether the material receives scene lighting.
        bool ReceivesLighting;

        /// Stores whether one cooked material explicitly authored double-sided rendering.
        bool DoubleSided;

        /// Stores whether one cooked material explicitly authored the double-sided setting.
        bool HasAuthoredDoubleSided;

        /// Stores whether one cooked lighting buffer explicitly authored the local lighting configuration.
        bool HasAuthoredLightingConfiguration;

        /// Stores the PSP lighting-response mode.
        PspMaterialLightingResponse LightingResponse;

        /// Stores the PSP texture loaded from this material's cooked texture reference for later renderer-managed release.
        RuntimeTexture* OwnedTexture;
    };
}
