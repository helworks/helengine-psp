# PSP Colored Cube Grid Fixed-Function Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `colored_cube_grid` render through the current PSP fixed-function lighting path and preserve authored solid colors plus directional lighting across the full cube grid.

**Architecture:** Keep the authored material schema unchanged and keep `FixedFunctionLambert` forced as the active PSP backend. Start with verification against the real `colored_cube_grid` startup scene, then only change the renderer if the scene exposes a true multi-draw fixed-function bug such as material state leakage or inconsistent lighting state across repeated draws.

**Tech Stack:** C++, PSP GU fixed-function rendering, C# PSP builder tests, Docker PSP toolchain, PPSSPP

---

## File Map

- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`
  - Owns fixed-function draw submission, GU lighting state, and per-draw material binding
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`
  - Owns renderer-level documentation if behavior or role description changes
- Modify: `src/platform/psp/rendering/PspLightingSettings.hpp`
  - Owns the currently forced fixed-function pipeline default
- Test: `builder.tests/CityColoredCubeGridSceneTests.cs`
  - Owns PSP-side scene/material regression coverage when a real contract gap is discovered
- Verify externally:
  - `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE`
  - `C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe`

## Task 1: Verify The Existing Fixed-Function Path Against Colored Cube Grid

**Files:**
- Verify: `builder.tests/helengine.psp.builder.tests.csproj`
- Verify: `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE`
- Verify: `C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe`

- [ ] **Step 1: Run the existing PSP builder regression suite before touching code**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal
```

Expected:
- PASS
- current managed PSP build/test surface stays green before scene verification begins

- [ ] **Step 2: Produce a fresh PSP export that targets `colored_cube_grid`**

Run:

```powershell
& {
    $env:HELENGINE_PSP_REPOSITORY_ROOT = 'C:\dev\helworks\helengine-psp'
    dotnet 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll' --project 'C:\dev\helprojs\city' --build psp --output 'C:\tmp\city-psp-colored-cube-grid-fixed-function-build'
}
```

Expected:
- a fresh PSP build output exists under:
  - `C:\tmp\city-psp-colored-cube-grid-fixed-function-build\PSP\GAME\HELENGINE`
- the cooked scene payload includes:
  - `cooked\scenes\rendering\colored_cube_grid.hasset`

- [ ] **Step 3: Normalize the generated-core root selected by that export**

Run:

```powershell
rtk dotnet run --project builder/helengine.psp.builder.csproj -- --normalize-generated-core <generated-core-root>
```

Expected:
- PASS
- generated-core compatibility rewrites complete cleanly

- [ ] **Step 4: Rebuild PSP `EBOOT.PBP` against the normalized generated-core root**

Run:

```powershell
rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v <generated-core-root>:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

Expected:
- PASS
- `build/EBOOT.PBP` is rebuilt with the current fixed-function renderer

- [ ] **Step 5: Install the rebuilt executable plus the fresh `colored_cube_grid` payload into PPSSPP**

Run:

```powershell
& {
    $memstickRoot = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE'
    $packageRoot = 'C:\tmp\city-psp-colored-cube-grid-fixed-function-build\PSP\GAME\HELENGINE'

    New-Item -ItemType Directory -Force -Path $memstickRoot | Out-Null
    Copy-Item 'C:\dev\helworks\helengine-psp\build\EBOOT.PBP' (Join-Path $memstickRoot 'EBOOT.PBP') -Force
    Copy-Item (Join-Path $packageRoot '*') $memstickRoot -Recurse -Force
}
```

Expected:
- `EBOOT.PBP` and the fresh cooked `colored_cube_grid` payload are installed in PPSSPP memstick

- [ ] **Step 6: Launch PPSSPP, capture logs and screenshot, then stop and ask the user what they see**

Run:

```powershell
& {
    $ppssppPath = 'C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe'
    $ebootPath = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP'
    Start-Process -FilePath $ppssppPath -ArgumentList ('"' + $ebootPath + '"') | Out-Null
}
```

Expected:
- PPSSPP launches the new build
- before drawing any conclusion, ask the user to describe exactly what they see

- [ ] **Step 7: Read the runtime result only after the user’s visual report**

Run:

```powershell
Get-Content 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log'
```

Expected:
- `RuntimeMainLoop` is reached
- if the user reports correct colored cubes and stable lighting, stop here and do not invent renderer changes

## Task 2: Add The Smallest Regression Only If Colored Cube Grid Exposes A Real Contract Gap

**Files:**
- Modify: `builder.tests/CityColoredCubeGridSceneTests.cs`

- [ ] **Step 1: Write a failing regression only if the scene failure reveals a real cook/runtime contract issue**

Add a concrete regression in `builder.tests/CityColoredCubeGridSceneTests.cs` only if the observed failure points to:
- wrong PSP material color payload
- missing PSP material settings
- wrong authored scene payload contract

Do not add a fake test for GU runtime state that the builder cannot observe.

- [ ] **Step 2: Run only the new failing regression and verify it fails for the expected reason**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~CityColoredCubeGridSceneTests -v minimal
```

Expected:
- FAIL for the exact scene/material contract bug being fixed

- [ ] **Step 3: Implement the smallest contract fix that makes the failing regression pass**

Allowed fix classes:
- scene material settings mismatch
- PSP builder/platform definition mismatch
- cooked material data mismatch

Not allowed:
- scene-specific runtime hacks in the PSP renderer

- [ ] **Step 4: Re-run the focused regression and verify it passes**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~CityColoredCubeGridSceneTests -v minimal
```

Expected:
- PASS

## Task 3: Fix The Fixed-Function Multi-Draw Renderer Only If The Scene Reveals A Runtime State Bug

**Files:**
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`
- Modify: `src/platform/psp/rendering/PspLightingSettings.hpp`

- [ ] **Step 1: Identify the exact runtime failure boundary from the user report plus PPSSPP evidence**

Classify the failure as one of:
- all cubes white
- some cube colors wrong
- lighting direction inconsistent across cubes
- transform/normal behavior inconsistent across instances
- draw-state leak between materials

Do not start editing until one boundary is chosen.

- [ ] **Step 2: Patch only the exact fixed-function state problem**

Examples of acceptable targeted fixes:
- move material color binding so it is refreshed for every draw
- move scene-light binding so it is configured once per camera pass
- clear or reapply GU lighting/material state that leaks between draws
- correct the normal or transform path only if the evidence points there

Do not:
- add textures
- add backend switching
- change authored material schema
- reintroduce CPU lighting for this milestone

- [ ] **Step 3: Rebuild the PSP executable after the targeted renderer fix**

Run:

```powershell
rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v <generated-core-root>:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

Expected:
- PASS

- [ ] **Step 4: Reinstall the same `colored_cube_grid` payload and relaunch PPSSPP**

Run the same install and PPSSPP launch flow from Task 1.

Expected:
- PPSSPP launches the updated renderer against the same scene payload
- stop and ask the user what they see after launch

## Task 4: Final Verification And Commit

**Files:**
- Modify: `README.md` only if the milestone meaningfully changes documented rendering status
- Commit only the files required by the actual fix

- [ ] **Step 1: Re-run the full PSP builder regression suite if any code changed**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal
```

Expected:
- PASS

- [ ] **Step 2: Confirm the final PPSSPP run with the user before claiming success**

Required:
- after the final PPSSPP launch, ask the user what they see
- only mark the milestone complete after the user confirms the visual result

- [ ] **Step 3: Read the final boot log and confirm runtime startup remained healthy**

Run:

```powershell
Get-Content 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log'
```

Expected:
- startup scene path points to `colored_cube_grid`
- `RuntimeMainLoop` is reached

- [ ] **Step 4: Commit only the files required by the real fix**

Possible commit shapes:
- verification-only milestone: no code commit if nothing changed
- builder/scene contract fix: commit only the test plus contract files
- runtime renderer fix: commit only the renderer files, plus docs only if updated

Commit message examples:

```bash
git commit -m "Fix PSP fixed-function colored cube grid materials"
```

or

```bash
git commit -m "Fix PSP fixed-function colored cube grid lighting state"
```

## Failure Boundaries

- startup failure before `RuntimeMainLoop`
- scene loads but cubes are white
- scene loads but only some colors are correct
- directional light or material state is inconsistent across cubes
- transforms or normals diverge across instances

## Exit Criteria

- `colored_cube_grid` reaches `RuntimeMainLoop`
- the user confirms the scene visually after PPSSPP launch
- cubes preserve authored solid colors
- directional lighting is stable across the full grid
- the milestone remains fixed-function only
