$ErrorActionPreference = 'Stop'

$path = 'C:\dev\helprojs\city\assets\codebase\menu\DemoDiscReturnToMenuComponent.cs'
$content = Get-Content -LiteralPath $path -Raw
$old = @"
            InputGamepadState currentGamepadState = ReadPrimaryGamepadState(inputSystem);
            if (!currentGamepadState.Connected) {
                PreviousGamepadState = currentGamepadState;
                return false;
            }

            return WasGamepadButtonPressed(currentGamepadState, PreviousGamepadState, InputGamepadButton.East)
                || WasGamepadButtonPressed(currentGamepadState, PreviousGamepadState, InputGamepadButton.North)
                || WasGamepadButtonPressed(currentGamepadState, PreviousGamepadState, InputGamepadButton.Select);
"@
$new = @"
            InputGamepadState currentGamepadState = ReadPrimaryGamepadState(inputSystem);
            if (!currentGamepadState.Connected) {
                PreviousGamepadState = currentGamepadState;
                return false;
            }

            bool wasReturnPressed =
                WasGamepadButtonPressed(currentGamepadState, PreviousGamepadState, InputGamepadButton.East)
                || WasGamepadButtonPressed(currentGamepadState, PreviousGamepadState, InputGamepadButton.North)
                || WasGamepadButtonPressed(currentGamepadState, PreviousGamepadState, InputGamepadButton.Select);
            PreviousGamepadState = currentGamepadState;
            return wasReturnPressed;
"@

if (-not $content.Contains($old)) {
    throw "Expected return-button block was not found in $path."
}

$updated = $content.Replace($old, $new)
Set-Content -LiteralPath $path -Value $updated
