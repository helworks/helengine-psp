#include "platform/psp/PspInputBackend.hpp"

#include <pspctrl.h>

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
        sceCtrlSetSamplingCycle(0);
        sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

        SceCtrlData padData {};
        sceCtrlPeekBufferPositive(&padData, 1);

        InputFrameState frameState;
        frameState.set_GamepadCount(1);

        Array<InputGamepadState>* gamepads = new Array<InputGamepadState>(1);
        InputGamepadState gamepadState;
        gamepadState.set_Connected(true);
        gamepadState.SetButtonDown(InputGamepadButton::DPadUp, (padData.Buttons & PSP_CTRL_UP) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadDown, (padData.Buttons & PSP_CTRL_DOWN) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadLeft, (padData.Buttons & PSP_CTRL_LEFT) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadRight, (padData.Buttons & PSP_CTRL_RIGHT) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::South, (padData.Buttons & PSP_CTRL_CROSS) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::East, (padData.Buttons & PSP_CTRL_CIRCLE) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::West, (padData.Buttons & PSP_CTRL_SQUARE) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::North, (padData.Buttons & PSP_CTRL_TRIANGLE) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::LeftShoulder, (padData.Buttons & PSP_CTRL_LTRIGGER) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::RightShoulder, (padData.Buttons & PSP_CTRL_RTRIGGER) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::Start, (padData.Buttons & PSP_CTRL_START) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::Select, (padData.Buttons & PSP_CTRL_SELECT) != 0);
        gamepadState.set_LeftStickX(static_cast<int16_t>((static_cast<int32_t>(padData.Lx) - 128) * 256));
        gamepadState.set_LeftStickY(static_cast<int16_t>((static_cast<int32_t>(padData.Ly) - 128) * 256));

        (*gamepads)[0] = gamepadState;
        frameState.set_Gamepads(gamepads);
        return frameState;
    }
}
