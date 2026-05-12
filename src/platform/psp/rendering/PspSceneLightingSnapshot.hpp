#pragma once

#include "float3.hpp"
#include "float4.hpp"

namespace helengine::psp::rendering {
    /// Stores the live scene-light data resolved for one PSP camera render pass.
    struct PspSceneLightingSnapshot {
        /// Whether a usable directional light was found in the scene.
        bool HasDirectionalLight = false;

        /// World-space light direction used for diffuse lighting.
        float3 DirectionalLightDirection = float3(0.0f, 0.0f, -1.0f);

        /// Authored directional-light color.
        float4 DirectionalLightColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

        /// Authored directional-light intensity.
        float DirectionalLightIntensity = 1.0f;
    };
}
