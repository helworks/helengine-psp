#include "platform/psp/PspInputBackend.hpp"

namespace helengine::psp {
    /// Creates the PSP input backend with background input disabled.
    PspInputBackend::PspInputBackend()
        : ReceiveInputInBackground(false) {
    }

    /// Returns whether the backend continues reporting input while the app is inactive.
    bool PspInputBackend::get_ReceiveInputInBackground() {
        return ReceiveInputInBackground;
    }

    /// Updates whether the backend continues reporting input while the app is inactive.
    void PspInputBackend::set_ReceiveInputInBackground(bool value) {
        ReceiveInputInBackground = value;
    }

    /// Captures one input frame from the PSP runtime.
    InputFrameState PspInputBackend::CaptureFrame() {
        return InputFrameState();
    }
}
