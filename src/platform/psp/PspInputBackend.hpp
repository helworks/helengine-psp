#pragma once

#include <cstdint>

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

    private:
        /// Stores the two reusable one-slot gamepad arrays that alternate between current and previous frame ownership.
        Array<InputGamepadState>* GamepadBuffers[2];

        /// Stores the buffer index that should back the next captured frame.
        std::int32_t ActiveGamepadBufferIndex;
    };
}
