#pragma once

#include "IInputBackend.hpp"
#include "InputFrameState.hpp"

namespace helengine::psp {
    /// Captures the minimal PSP input frame required to initialize generated core.
    class PspInputBackend final : public IInputBackend {
    public:
        /// Creates the PSP input backend.
        PspInputBackend();

        /// Captures one input frame from the PSP runtime.
        InputFrameState CaptureFrame() override;
    };
}
