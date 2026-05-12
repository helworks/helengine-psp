#pragma once

namespace helengine::psp::rendering {
    /// Selects the PSP lighting backend used by the 3D renderer.
    enum class PspLightingPipeline {
        CpuVertexLambert = 0,
        FixedFunctionLambert = 1
    };
}
