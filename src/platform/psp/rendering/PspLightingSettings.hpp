#pragma once

#include "platform/psp/rendering/PspLightingPipeline.hpp"

namespace helengine::psp::rendering {
    /// Stores renderer-owned PSP lighting defaults and backend choices.
    struct PspLightingSettings {
        /// Default ambient intensity used until ambient becomes scene-authored.
        float AmbientIntensity = 0.25f;

        /// Active PSP lighting pipeline.
        PspLightingPipeline Pipeline = PspLightingPipeline::CpuVertexLambert;
    };
}
