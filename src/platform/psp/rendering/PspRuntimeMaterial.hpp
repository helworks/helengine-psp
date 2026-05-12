#pragma once

#include <cstdint>

#include "MaterialAsset.hpp"
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

        /// Gets the PSP lighting-response mode.
        PspMaterialLightingResponse GetLightingResponse() const;

        /// Resolves the first bound PSP runtime texture when the material exposes one.
        bool TryResolveTexture(PspRuntimeTexture*& texture);

        /// Loads PSP material state from one cooked material asset.
        void LoadFromCooked(MaterialAsset* materialAsset);

    private:
        /// Stores the authored base color.
        float4 BaseColor;

        /// Stores whether the material receives scene lighting.
        bool ReceivesLighting;

        /// Stores the PSP lighting-response mode.
        PspMaterialLightingResponse LightingResponse;
    };
}
