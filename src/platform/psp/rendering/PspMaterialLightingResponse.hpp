#pragma once

namespace helengine::psp::rendering {
    /// Describes how one PSP runtime material should react to scene lighting.
    enum class PspMaterialLightingResponse {
        Unlit = 0,
        LitDirectional = 1
    };
}
