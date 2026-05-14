# Helengine PSP Axis Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Verify the real city `axis_test` scene boots and renders correctly on PSP in PPSSPP using the current solid-color directional-lighting renderer path, and stop at the first concrete compatibility boundary if it does not.

**Architecture:** This is a verification-first plan, not a speculative renderer-expansion plan. It uses the existing PSP runtime contract, switches the local city PSP startup-scene configuration to `axis_test`, exports a fresh payload, normalizes the fresh generated-core root, rebuilds `EBOOT.PBP`, and verifies the real authored scene in PPSSPP. Only if the scene fails do we capture the exact failure boundary and prepare a focused follow-up fix path.

**Tech Stack:** .NET 9, xUnit, PowerShell, Docker, PSPDEV/PSPSDK, PPSSPP, GNU Make, CMake

---

## File Structure

- Modify (local-only, do not commit unless explicitly requested): `C:\dev\helprojs\city\user_settings\build_config.json`
  Purpose: point the city PSP build target at `axis_test` and a dedicated output folder for this verification run.
- Create (temporary local backup): `C:\tmp\city-build-config-axis-test-backup.json`
  Purpose: preserve the previous city build configuration so the local PSP scene selection can be restored after verification.
- Produce (generated output): `C:\tmp\city-psp-axis-test-build\PSP\GAME\HELENGINE\...`
  Purpose: hold the freshly exported city PSP payload for `axis_test`.
- Produce (temporary evidence, do not commit): `C:\tmp\ppsspp-axis-test.png`
  Purpose: capture the PPSSPP frame used to verify visible `axis_test` rendering.
- Produce (runtime log, do not commit): `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`
  Purpose: confirm the exact PSP runtime stage reached during the verification run.

### Task 1: Prepare The Local PSP Axis Test Verification Configuration

**Files:**
- Modify (local-only): `C:\dev\helprojs\city\user_settings\build_config.json`
- Create (temporary): `C:\tmp\city-build-config-axis-test-backup.json`

- [ ] **Step 1: Back up the current local city build configuration**

Run:

```powershell
Copy-Item 'C:\dev\helprojs\city\user_settings\build_config.json' 'C:\tmp\city-build-config-axis-test-backup.json' -Force
```

Expected: PASS with `C:\tmp\city-build-config-axis-test-backup.json` containing the pre-existing city build configuration.

- [ ] **Step 2: Rewrite the PSP build target to boot `axis_test` into a dedicated output folder**

Run:

```powershell
& {
    $buildConfigPath = 'C:\dev\helprojs\city\user_settings\build_config.json'
    $buildConfig = Get-Content $buildConfigPath -Raw | ConvertFrom-Json
    $pspPlatform = $buildConfig.platforms | Where-Object { $_.platformId -eq 'psp' }
    if ($null -eq $pspPlatform) {
        throw 'PSP platform configuration was not found in city build_config.json.'
    }

    $pspPlatform.selectedSceneIds = @('axis_test')
    $pspPlatform.sceneOrders = @(
        [pscustomobject]@{
            orderNumber = 1
            sceneId = 'axis_test'
        }
    )
    $pspPlatform.outputDirectoryPath = 'C:\tmp\city-psp-axis-test-build'

    $buildConfig | ConvertTo-Json -Depth 100 | Set-Content $buildConfigPath
}
```

Expected: PASS with the PSP platform entry now targeting only `axis_test` and `C:\tmp\city-psp-axis-test-build`.

- [ ] **Step 3: Verify the local PSP build configuration actually points to `axis_test`**

Run:

```powershell
Get-Content 'C:\dev\helprojs\city\user_settings\build_config.json' | Select-String '"platformId":  "psp"|"axis_test"|"C:\\tmp\\city-psp-axis-test-build"'
```

Expected: PASS with matches showing the PSP platform block now references `axis_test` and `C:\tmp\city-psp-axis-test-build`.

### Task 2: Export A Fresh City PSP Axis Test Payload And Rebuild The PSP Executable

**Files:**
- Produce: `C:\tmp\city-psp-axis-test-build\PSP\GAME\HELENGINE\...`
- Use: `builder/helengine.psp.builder.csproj`
- Use: `build/EBOOT.PBP`

- [ ] **Step 1: Run the current PSP builder test suite before runtime verification**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal
```

Expected: PASS with the PSP builder suite green before the `axis_test` run.

- [ ] **Step 2: Export a fresh city PSP build for `axis_test`**

Run:

```powershell
& {
    $env:HELENGINE_PSP_REPOSITORY_ROOT = 'C:\dev\helworks\helengine-psp'
    dotnet 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll' --project 'C:\dev\helprojs\city' --build psp --output 'C:\tmp\city-psp-axis-test-build'
}
```

Expected: PASS with the editor reporting a completed PSP build under `C:\tmp\city-psp-axis-test-build`.

- [ ] **Step 3: Verify the exported payload contains the authored `axis_test` scene**

Run:

```powershell
Get-ChildItem 'C:\tmp\city-psp-axis-test-build\PSP\GAME\HELENGINE\cooked\scenes\rendering'
```

Expected: PASS with `axis_test.hasset` present in the exported cooked scene folder.

- [ ] **Step 4: Identify the newest generated-core root produced by the export**

Run:

```powershell
Get-ChildItem 'C:\Users\Helena\AppData\Local\Temp\helengine-platform-build\psp' -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1 FullName, LastWriteTime
```

Expected: PASS with a single latest PSP temp build root, such as `C:\Users\Helena\AppData\Local\Temp\helengine-platform-build\psp\<guid>`.

- [ ] **Step 5: Normalize the fresh generated-core root with the PSP compatibility normalizer**

Run:

```powershell
& {
    $latestRoot = Get-ChildItem 'C:\Users\Helena\AppData\Local\Temp\helengine-platform-build\psp' -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    $generatedCoreRoot = Join-Path $latestRoot 'generated-core'
    rtk dotnet run --project builder/helengine.psp.builder.csproj -- --normalize-generated-core $generatedCoreRoot
}
```

Expected: PASS with the builder reporting `Normalized generated-core compatibility rewrites under: <latest generated-core root>`.

- [ ] **Step 6: Rebuild `EBOOT.PBP` against that fresh normalized generated-core root**

Run:

```powershell
& {
    $latestRoot = Get-ChildItem 'C:\Users\Helena\AppData\Local\Temp\helengine-platform-build\psp' -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    $generatedCoreRoot = (Join-Path $latestRoot 'generated-core').Replace('\', '/')
    rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v ${generatedCoreRoot}:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
}
```

Expected: PASS with `EBOOT.PBP file created for target helengine_psp.` in the build output.

### Task 3: Install The Axis Test Payload Into PPSSPP And Verify The Real Scene

**Files:**
- Produce: `C:\tmp\ppsspp-axis-test.png`
- Produce: `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`

- [ ] **Step 1: Install the rebuilt PSP executable and the fresh `axis_test` cooked payload into the PPSSPP memstick tree**

Run:

```powershell
& {
    $memstickRoot = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE'
    Copy-Item 'C:\dev\helworks\helengine-psp\build\EBOOT.PBP' (Join-Path $memstickRoot 'EBOOT.PBP') -Force
    Copy-Item 'C:\tmp\city-psp-axis-test-build\PSP\GAME\HELENGINE\cooked\*' (Join-Path $memstickRoot 'cooked') -Recurse -Force
}
```

Expected: PASS with the PPSSPP memstick tree containing the latest executable and `axis_test` cooked content.

- [ ] **Step 2: Launch PPSSPP, clear the old boot log, and capture a fresh screenshot**

Run:

```powershell
& {
    $ErrorActionPreference = 'Stop'
    $memstickRoot = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE'
    $ppssppExe = 'C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe'
    $bootLogPath = Join-Path $memstickRoot 'helengine_psp_boot.log'
    $screenshotPath = 'C:\tmp\ppsspp-axis-test.png'

    Stop-Process -Name 'PPSSPPWindows64' -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    if (Test-Path $bootLogPath) {
        Remove-Item $bootLogPath -Force
    }

    $process = Start-Process $ppssppExe -ArgumentList (Join-Path $memstickRoot 'EBOOT.PBP') -PassThru
    Start-Sleep -Seconds 10

    Add-Type -AssemblyName System.Drawing
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class NativeWindowCapture {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
}
"@

    $deadline = (Get-Date).AddSeconds(20)
    while ($process.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
    }

    if ($process.MainWindowHandle -eq 0) {
        throw 'PPSSPP main window was not created.'
    }

    [void][NativeWindowCapture]::SetForegroundWindow($process.MainWindowHandle)
    Start-Sleep -Seconds 1

    $rect = New-Object NativeWindowCapture+RECT
    if (-not [NativeWindowCapture]::GetWindowRect($process.MainWindowHandle, [ref]$rect)) {
        throw 'Failed to get PPSSPP window bounds.'
    }

    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) {
        throw "Invalid PPSSPP window size: ${width}x${height}"
    }

    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
    $bitmap.Save($screenshotPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()
}
```

Expected: PASS with `C:\tmp\ppsspp-axis-test.png` created and a fresh boot log generated in the memstick game folder.

- [ ] **Step 3: Check that PSP startup reached the main loop**

Run:

```powershell
Get-Content 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log' -Tail 80
```

Expected: PASS with `Stage complete RuntimeMainLoop` present in the log tail.

- [ ] **Step 4: Inspect the screenshot for authored `axis_test` scene correctness**

Open:

```text
C:\tmp\ppsspp-axis-test.png
```

Expected visual result:

- floor and ground geometry are visible
- X axis is red
- Y axis is green
- Z axis is blue
- origin and axis markers are white
- the scene is directionally lit rather than flat or black
- the authored camera frames the scene clearly enough to inspect

### Task 4: Success Path Cleanup When Axis Test Already Works

**Files:**
- Modify (restore local-only file): `C:\dev\helprojs\city\user_settings\build_config.json`
- Delete (temporary local backup): `C:\tmp\city-build-config-axis-test-backup.json`

- [ ] **Step 1: Restore the previous local city build configuration**

Run:

```powershell
Copy-Item 'C:\tmp\city-build-config-axis-test-backup.json' 'C:\dev\helprojs\city\user_settings\build_config.json' -Force
```

Expected: PASS with the city build configuration restored to its pre-`axis_test` state.

- [ ] **Step 2: Confirm the local PSP build configuration no longer points at `axis_test`**

Run:

```powershell
Get-Content 'C:\dev\helprojs\city\user_settings\build_config.json' | Select-String '"platformId":  "psp"|"textured_cube_grid"|"axis_test"'
```

Expected: PASS with the PSP block restored to its previous scene selection, and `axis_test` no longer selected unless it was already present before this milestone.

- [ ] **Step 3: Remove the temporary config backup**

Run:

```powershell
Remove-Item 'C:\tmp\city-build-config-axis-test-backup.json' -Force
```

Expected: PASS with the temporary backup file removed.

- [ ] **Step 4: Do not create an engine commit if no PSP source files changed**

Run:

```powershell
git status --short
```

Expected: PASS with no new PSP engine source changes produced by this verification-only milestone. Temporary screenshots, logs, and plan files should remain uncommitted unless explicitly requested.

### Task 5: Failure Path If Axis Test Exposes A Real Compatibility Gap

**Files:**
- Produce (temporary evidence): `C:\tmp\ppsspp-axis-test.png`
- Produce (runtime evidence): `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`

- [ ] **Step 1: Record the first concrete failure boundary instead of guessing at fixes**

Inspect:

```text
C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log
```

Expected output to capture:

- `RuntimeStartupSceneAssetLoad`
- `RuntimeStartupSceneMaterialization`
- `RuntimeMainLoop`
- or the first earlier stage that fails instead

- [ ] **Step 2: Record the visible symptom from the screenshot**

Inspect:

```text
C:\tmp\ppsspp-axis-test.png
```

Expected evidence to capture:

- black screen
- wrong colors
- missing geometry
- flat unlit output
- incorrect framing
- or another concrete visible failure

- [ ] **Step 3: Restore the local city build configuration even on failure**

Run:

```powershell
Copy-Item 'C:\tmp\city-build-config-axis-test-backup.json' 'C:\dev\helprojs\city\user_settings\build_config.json' -Force
```

Expected: PASS with the local city configuration returned to its previous state before deeper debugging begins.

- [ ] **Step 4: Stop and write a focused follow-up spec from the actual failure boundary**

Run:

```powershell
Write-Output 'Stop execution here. Use the recorded axis_test boundary and visible symptom to write a focused follow-up spec/plan for the actual compatibility bug.'
```

Expected: PASS with no speculative PSP renderer edits made before the real failure is understood.
