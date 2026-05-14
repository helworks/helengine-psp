#pragma once

#include "IInputBackend.hpp"
#include "InputFrameState.hpp"

namespace helengine::psp {
    /// Captures the minimal PSP input frame required to initialize generated core.
    class PspInputBackend final : public IInputBackend {
    public:
        /// Creates the PSP input backend with background input disabled.
        PspInputBackend();

        /// Returns whether the backend continues reporting input while the app is inactive.
        bool get_ReceiveInputInBackground() override;

        /// Updates whether the backend continues reporting input while the app is inactive.
        void set_ReceiveInputInBackground(bool value) override;

        /// Captures one input frame from the PSP runtime.
        InputFrameState CaptureFrame() override;

    private:
        /// Tracks whether background input capture is enabled.
        bool ReceiveInputInBackground;
    };
}
