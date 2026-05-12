#pragma once

#include "RuntimeModel.hpp"

namespace helengine::psp::rendering {
    /// Exposes one concrete PSP runtime-model type so generated core can instantiate authored mesh data for the renderer.
    class PspRuntimeModel final : public RuntimeModel {
    public:
        /// Creates one PSP runtime-model instance.
        PspRuntimeModel();
    };
}
