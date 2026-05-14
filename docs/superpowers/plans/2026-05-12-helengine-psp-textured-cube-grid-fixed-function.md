# PSP Textured Cube Grid Fixed-Function Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `textured_cube_grid` render through the PSP fixed-function pipeline with authored textures, directional lighting, ambient fill, and correct base-color modulation.

**Architecture:** Keep `FixedFunctionLambert` forced as the active PSP backend and extend that branch so textured materials no longer throw. Start with the real `textured_cube_grid` export and verification path, then patch only the actual fixed-function textured state or vertex-layout problem the scene exposes.

**Tech Stack:** C++, PSP GU fixed-function rendering, C# PSP builder tests, Docker PSP toolchain, PPSSPP

---

## File Map

- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`
  - Owns the fixed-function textured vertex layout, per-draw texture binding, and fixed-function light/material state
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`
  - Owns renderer-level documentation if the class role description changes
- Modify: `src/platform/psp/rendering/PspLightingSettings.hpp`
  - Owns the forced fixed-function pipeline default if any milestone note changes
- Modify: `builder.tests/CityTexturedCubeGridSceneTests.cs`
  - Owns scene/material contract regressions only if a real authored or cooked contract gap is discovered
- Modify externally: `C:\dev\helprojs\city\user_settings\build_config.json`
  - Owns the current PSP export startup-scene selection
- Verify externally:
  - `C:\tmp\city-psp-textured-cube-grid-fixed-function-build`
  - `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE`
  - `C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe`

## Task 1: Retarget The PSP Export And Verify The Fresh Scene Package

**Files:**
- Modify externally: `C:\dev\helprojs\city\user_settings\build_config.json`
- Verify: `builder.tests/helengine.psp.builder.tests.csproj`

- [ ] **Step 1: Run the PSP builder regression suite before changing scene selection**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal
```

Expected:
- PASS
- the managed PSP build/test surface is green before scene verification starts

- [ ] **Step 2: Retarget the city PSP build config to `textured_cube_grid`**

In `C:\dev\helprojs\city\user_settings\build_config.json`, update the PSP platform block so it contains:

```json
"platformId":  "psp",
"selectedSceneIds":  [
    "textured_cube_grid"
],
"sceneOrders":  [
    {
        "orderNumber":  1,
        "sceneId":  "textured_cube_grid"
    }
],
"outputDirectoryPath":  "C:\\tmp\\city-psp-textured-cube-grid-fixed-function-build"
```

Expected:
- only the PSP block changes
- Windows and PS2 scene selection stay untouched

- [ ] **Step 3: Produce a fresh editor-driven PSP export for `textured_cube_grid`**

Run:

```powershell
& {
    $env:HELENGINE_PSP_REPOSITORY_ROOT = 'C:\dev\helworks\helengine-psp'
    dotnet 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll' --project 'C:\dev\helprojs\city' --build psp --output 'C:\tmp\city-psp-textured-cube-grid-fixed-function-build'
}
```

Expected:
- `C:\tmp\city-psp-textured-cube-grid-fixed-function-build\PSP\GAME\HELENGINE\cooked\scenes\rendering\textured_cube_grid.hasset` exists
- the export root contains the scene payload intended for this milestone

- [ ] **Step 4: Locate the newest PSP build root and verify the generated startup manifest now points at `textured_cube_grid`**

Run:

```powershell
Get-ChildItem 'C:\Users\Helena\AppData\Local\Temp\helengine-platform-build\psp' -Directory |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 3 FullName, LastWriteTime
```

Then read the newest root’s startup manifest:

```powershell
Get-Content '<newest-build-root>\generated-core\runtime\runtime_startup_manifest.cpp'
```

Expected:
- the newest build root contains:
  - `static const char kRuntimeStartupSceneRelativePath[] = "cooked/scenes/rendering/textured_cube_grid.hasset";`

## Task 2: Extend Fixed-Function Submission To Support Textured Materials

**Files:**
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`

- [ ] **Step 1: Identify the current fixed-function texture failure boundary in the renderer**

Verify that `src/platform/psp/rendering/PspRenderManager3D.cpp` still contains the fixed-function textured-material throw:

```cpp
if (LightingSettings.Pipeline == PspLightingPipeline::FixedFunctionLambert) {
    if (hasTexture) {
        throw std::runtime_error("PSP fixed-function lighting does not support textured materials yet.");
    }
```

Expected:
- the failure boundary is explicit
- this is the exact branch to replace for the milestone

- [ ] **Step 2: Replace the fixed-function textured throw with a dedicated textured fixed-function submission path**

Implement a textured fixed-function vertex layout in `src/platform/psp/rendering/PspRenderManager3D.cpp` that carries:
- UVs
- normals
- positions

The fixed-function textured branch must:
- keep using `PspRuntimeMaterial::TryResolveTexture(...)`
- bind the resolved `PspRuntimeTexture`
- submit GU vertices with UVs, normals, and positions
- preserve the current untextured fixed-function path for solid-color scenes

Do not:
- reintroduce CPU textured lighting
- add backend switching
- change authored material schema

- [ ] **Step 3: Configure GU texture state so it coexists with the fixed-function light/material state**

In `src/platform/psp/rendering/PspRenderManager3D.cpp`, ensure the textured fixed-function path:
- enables `GU_TEXTURE_2D`
- uses the existing nearest sampling behavior
- uses a texture function that modulates texels with material color and lighting
- avoids breaking the current untextured fixed-function path

The intended milestone outcome is:
- sampled texture * base color * lighting

- [ ] **Step 4: Refresh per-draw texture state and material state explicitly**

In the textured fixed-function branch, make per-draw state application explicit so repeated cubes do not leak:
- texture binding
- material color state
- lighting enable state

Expected:
- no texture sharing bug between cubes
- no white/black washout caused by stale GU state

## Task 3: Add Only The Smallest Regression Surface If A Real Contract Gap Appears

**Files:**
- Modify: `builder.tests/CityTexturedCubeGridSceneTests.cs`

- [ ] **Step 1: Write a failing regression only if the first textured fixed-function run reveals an authored or cooked contract problem**

Allowed regression targets:
- wrong PSP texture-id material field
- missing PSP material settings in the textured scene
- wrong cooked texture/material payload contract

Do not add builder tests for runtime GU state.

- [ ] **Step 2: Run the focused textured-scene regression and verify it fails for the expected reason**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~CityTexturedCubeGridSceneTests -v minimal
```

Expected:
- FAIL only if a real contract gap was discovered

- [ ] **Step 3: Implement the smallest contract fix and rerun the focused regression**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~CityTexturedCubeGridSceneTests -v minimal
```

Expected:
- PASS

## Task 4: Rebuild, Install Correctly, And Verify In PPSSPP

**Files:**
- Verify externally:
  - `C:\tmp\city-psp-textured-cube-grid-fixed-function-build`
  - `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE`

- [ ] **Step 1: Normalize the newest `textured_cube_grid` generated-core root**

Run:

```powershell
rtk dotnet run --project builder/helengine.psp.builder.csproj -- --normalize-generated-core '<newest-build-root>\generated-core'
```

Expected:
- PASS

- [ ] **Step 2: Rebuild PSP `EBOOT.PBP` against the normalized `textured_cube_grid` generated-core root**

Run:

```powershell
rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v <newest-build-root>/generated-core:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

Expected:
- PASS
- `build/EBOOT.PBP` is rebuilt from the current fixed-function textured renderer

- [ ] **Step 3: Install the payload in the correct order so the rebuilt executable is not overwritten**

Run:

```powershell
& {
    $memstickRoot = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE'
    $packageRoot = 'C:\tmp\city-psp-textured-cube-grid-fixed-function-build\PSP\GAME\HELENGINE'

    New-Item -ItemType Directory -Force -Path $memstickRoot | Out-Null
    Copy-Item (Join-Path $packageRoot 'cooked') (Join-Path $memstickRoot 'cooked') -Recurse -Force
    if (Test-Path (Join-Path $packageRoot 'Materials')) {
        Copy-Item (Join-Path $packageRoot 'Materials') (Join-Path $memstickRoot 'Materials') -Recurse -Force
    }
    Copy-Item 'C:\dev\helworks\helengine-psp\build\EBOOT.PBP' (Join-Path $memstickRoot 'EBOOT.PBP') -Force
}
```

Expected:
- cooked assets are installed first
- rebuilt `EBOOT.PBP` is copied last
- the installed `EBOOT.PBP` timestamp matches the rebuilt local one

- [ ] **Step 4: Launch PPSSPP and stop for the user’s visual report before inspecting logs**

Run:

```powershell
& {
    $ppssppPath = 'C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe'
    $ebootPath = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP'
    Start-Process -FilePath $ppssppPath -ArgumentList ('"' + $ebootPath + '"') | Out-Null
}
```

Expected:
- PPSSPP launches the rebuilt `textured_cube_grid` scene
- stop and ask the user exactly what they see

- [ ] **Step 5: Read logs only after the user’s visual report**

Run:

```powershell
Get-Content 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log'
```

Expected:
- startup scene path points to `textured_cube_grid`
- `RuntimeMainLoop` is reached

## Task 5: Final Verification And Commit

**Files:**
- Modify: `README.md` only if the milestone changes documented PSP rendering status
- Commit only files required by the actual fix

- [ ] **Step 1: Re-run the full PSP builder regression suite if any source code changed**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal
```

Expected:
- PASS

- [ ] **Step 2: Confirm the final PPSSPP visual result with the user before claiming success**

Required:
- only mark the milestone complete after the user confirms what is on screen

- [ ] **Step 3: Update README only if the milestone lands successfully**

If fixed-function textured rendering works, update `README.md` so PSP rendering status reflects:
- fixed-function textured + directional support on the current milestone
- nearest sampling
- no mipmaps

If the milestone does not fully land, do not overstate support.

- [ ] **Step 4: Commit only the real fix**

Possible commit shapes:
- runtime renderer fix only
- renderer fix plus README
- contract regression plus contract fix plus renderer fix

Commit message examples:

```bash
git commit -m "Add PSP fixed-function textured lighting"
```

or

```bash
git commit -m "Fix PSP fixed-function textured material state"
```

## Failure Boundaries

- startup failure before `RuntimeMainLoop`
- textures missing entirely
- wrong or shared textures across cubes
- lighting disappears when textures are enabled
- textures and lighting both appear but modulate incorrectly
- texture or lighting state leaks between draws

## Exit Criteria

- `textured_cube_grid` reaches `RuntimeMainLoop`
- the user confirms the visual result after PPSSPP launch
- textures are correct and distinct
- directional lighting remains active
- base-color modulation behaves correctly
- the scene remains opaque
