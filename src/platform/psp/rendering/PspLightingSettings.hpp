#pragma once

#include "platform/psp/rendering/PspLightingPipeline.hpp"

namespace helengine::psp::rendering {
    /// Stores renderer-owned PSP lighting defaults and backend choices.
    struct PspLightingSettings {
        /// Default ambient intensity used until ambient becomes scene-authored.
        float AmbientIntensity = 0.25f;

        /// Active PSP lighting pipeline. Fixed-function is the default PSP lighting path for compatible scenes.
        PspLightingPipeline Pipeline = PspLightingPipeline::FixedFunctionLambert;
    };
}
