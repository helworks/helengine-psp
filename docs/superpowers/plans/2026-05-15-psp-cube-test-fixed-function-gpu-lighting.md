# PSP Cube Test Fixed-Function GPU Lighting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `cube_test` use an explicit PSP GU fixed-function directional-light path for the narrow compatible slice while keeping CPU lighting as the fallback for everything else.

**Architecture:** The PSP renderer already contains both a GU fixed-function submission path and a CPU-lit fallback path, but the current implementation treats `FixedFunctionLambert` as a broad pipeline switch instead of a narrow per-draw compatibility decision. This plan makes the compatibility slice explicit, hardens mixed GPU/CPU draw-state transitions so unsupported draws remain correct, and adds runtime evidence in the PSP profiler so `cube_test` can prove it is actually using the GPU path.

**Tech Stack:** C++, PSP GU (`pspgu`, `pspgum`), xUnit for existing builder safety checks, Docker-based PSP native build, PPSSPP for runtime verification.

---

## File Map

- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`
  - Add explicit helper declarations for GPU compatibility checks and CPU-fallback draw-state preparation.
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`
  - Make per-draw GPU compatibility explicit.
  - Route only the narrow `cube_test` slice through GU fixed-function directional lighting.
  - Route unsupported draws through CPU fallback with correct GU state reset.
- Modify: `src/platform/psp/rendering/PspRenderProfiler.hpp`
  - Add explicit counters for GPU-directional visits versus CPU-fallback visits.
- Modify: `src/platform/psp/rendering/PspRenderProfiler.cpp`
  - Track and emit the new path-selection counters in `PspPerfFrame3D`.
- Temporary verification only, never commit:
  - `C:\dev\helprojs\city\user_settings\build_config.json`
  - `C:\dev\helprojs\output\psp-cube-test-gpu-lighting`
  - `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`

## Task 1: Add Path-Selection Evidence

**Files:**
- Modify: `src/platform/psp/rendering/PspRenderProfiler.hpp`
- Modify: `src/platform/psp/rendering/PspRenderProfiler.cpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`

- [ ] **Step 1: Run the current `cube_test` baseline and confirm the profiler does not expose explicit GPU-versus-CPU path selection**

Run:

```powershell
rtk proxy powershell -Command @'
$ErrorActionPreference = "Stop"

$buildConfigPath = "C:\dev\helprojs\city\user_settings\build_config.json"
$backupPath = "$buildConfigPath.gpu-lighting-plan.bak"
Copy-Item -LiteralPath $buildConfigPath -Destination $backupPath -Force

try {
    $buildConfig = Get-Content -LiteralPath $buildConfigPath -Raw | ConvertFrom-Json
    $pspPlatform = $buildConfig.platforms | Where-Object { $_.platformId -eq "psp" }
    if ($null -eq $pspPlatform) {
        throw "PSP platform configuration was not found."
    }

    $pspPlatform.sceneOrders = @(
        [pscustomobject]@{ sceneId = "cube_test"; orderNumber = 1 },
        [pscustomobject]@{ sceneId = "DemoDiscMainMenu"; orderNumber = 2 },
        [pscustomobject]@{ sceneId = "colored_cube_grid"; orderNumber = 3 },
        [pscustomobject]@{ sceneId = "textured_cube_grid"; orderNumber = 4 },
        [pscustomobject]@{ sceneId = "axis_test"; orderNumber = 5 },
        [pscustomobject]@{ sceneId = "axis_test2"; orderNumber = 6 },
        [pscustomobject]@{ sceneId = "directional_shadow_plaza"; orderNumber = 7 },
        [pscustomobject]@{ sceneId = "spotlight_street_slice"; orderNumber = 8 }
    )

    $buildConfig | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $buildConfigPath

    rtk dotnet "C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll" --project "C:\dev\helprojs\city\project.heproj" --build psp --output "C:\dev\helprojs\output\psp-cube-test-gpu-lighting"

    $workspaceRoot = Get-ChildItem "$env:LOCALAPPDATA\Temp\helengine-platform-build\psp" -Directory | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($null -eq $workspaceRoot) {
        throw "No PSP generated-core workspace was found."
    }

    $generatedCoreRoot = Join-Path $workspaceRoot.FullName "generated-core"
    rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v "${generatedCoreRoot}:/generated-core" -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON HELENGINE_PSP_ENABLE_BOOT_TRACE=ON HELENGINE_PSP_ENABLE_RENDER_PROFILER=ON

    $sourceRoot = "C:\dev\helprojs\output\psp-cube-test-gpu-lighting\PSP\GAME\HELENGINE"
    $targetRoot = "C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE"
    if (Test-Path -LiteralPath $targetRoot) {
        Remove-Item -LiteralPath $targetRoot -Recurse -Force
    }

    Copy-Item -LiteralPath $sourceRoot -Destination $targetRoot -Recurse -Force
    Copy-Item -LiteralPath "C:\dev\helworks\helengine-psp\build\EBOOT.PBP" -Destination (Join-Path $targetRoot "EBOOT.PBP") -Force

    $bootLog = rtk proxy powershell -File C:\dev\helworks\helengine-psp\tools\run_ppsspp_boot_check.ps1
    $bootLog | Select-String "PspPerfFrame3D|LoadStartupScene|gpuDirectionalCount|cpuFallbackCount"
} finally {
    if (Test-Path -LiteralPath $backupPath) {
        Move-Item -LiteralPath $backupPath -Destination $buildConfigPath -Force
    }
}
'@
```

Expected: FAIL at the final `Select-String` check because the current `PspPerfFrame3D` line does not yet contain `gpuDirectionalCount` or `cpuFallbackCount`.

- [ ] **Step 2: Add explicit profiler counters for GPU-directional visits and CPU-fallback visits**

In `src/platform/psp/rendering/PspRenderProfiler.hpp`, add these declarations to the enabled and disabled profiler variants:

```cpp
        /// Records one drawable that used the narrow PSP GPU directional-light path.
        static void Record3DGpuDirectionalVisit();

        /// Records one drawable that used the CPU lighting fallback path.
        static void Record3DCpuFallbackVisit();
```

In `src/platform/psp/rendering/PspRenderProfiler.cpp`, extend `FrameProfileState`, reset logic, summary emission, and the concrete methods:

```cpp
            int32_t ThreeDGpuDirectionalVisitCount = 0;
            int32_t ThreeDCpuFallbackVisitCount = 0;
```

```cpp
            CurrentFrame.ThreeDGpuDirectionalVisitCount = 0;
            CurrentFrame.ThreeDCpuFallbackVisitCount = 0;
```

```cpp
                + " gpuDirectionalCount=" + std::to_string(CurrentFrame.ThreeDGpuDirectionalVisitCount)
                + " cpuFallbackCount=" + std::to_string(CurrentFrame.ThreeDCpuFallbackVisitCount)
```

```cpp
    void PspRenderProfiler::Record3DGpuDirectionalVisit() {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        CurrentFrame.ThreeDGpuDirectionalVisitCount++;
    }

    void PspRenderProfiler::Record3DCpuFallbackVisit() {
        if (!CurrentFrame.FrameActive) {
            return;
        }

        CurrentFrame.ThreeDCpuFallbackVisitCount++;
    }
```

- [ ] **Step 3: Record the existing path choice inside `PspRenderManager3D::Visit(...)` without changing the routing yet**

In `src/platform/psp/rendering/PspRenderManager3D.cpp`, keep the current behavior but add temporary path markers around the existing branches so the runtime log can show which branch `cube_test` is using before the compatibility rewrite:

```cpp
        if (LightingSettings.Pipeline == PspLightingPipeline::FixedFunctionLambert) {
            PspRenderProfiler::Record3DGpuDirectionalVisit();
            if (hasTexture) {
                SubmitFixedFunctionTexturedDrawable(pspRuntimeModelData, baseColor, useLighting, texture);
                PspRenderProfiler::Record3DVisit(PspRenderProfiler::GetTimestampMicroseconds() - visitStartMicroseconds);
                return;
            }

            SubmitFixedFunctionDrawable(pspRuntimeModelData, baseColor, useLighting);
            PspRenderProfiler::Record3DVisit(PspRenderProfiler::GetTimestampMicroseconds() - visitStartMicroseconds);
            return;
        }

        PspRenderProfiler::Record3DCpuFallbackVisit();
```

- [ ] **Step 4: Re-run the profiler build and verify the log now exposes path-selection counts**

Run:

```powershell
rtk proxy powershell -Command @'
$ErrorActionPreference = "Stop"

$workspaceRoot = Get-ChildItem "$env:LOCALAPPDATA\Temp\helengine-platform-build\psp" -Directory | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if ($null -eq $workspaceRoot) {
    throw "No PSP generated-core workspace was found."
}

$generatedCoreRoot = Join-Path $workspaceRoot.FullName "generated-core"
rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v "${generatedCoreRoot}:/generated-core" -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON HELENGINE_PSP_ENABLE_BOOT_TRACE=ON HELENGINE_PSP_ENABLE_RENDER_PROFILER=ON

$sourceRoot = "C:\dev\helprojs\output\psp-cube-test-gpu-lighting\PSP\GAME\HELENGINE"
$targetRoot = "C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE"
if (Test-Path -LiteralPath $targetRoot) {
    Remove-Item -LiteralPath $targetRoot -Recurse -Force
}

Copy-Item -LiteralPath $sourceRoot -Destination $targetRoot -Recurse -Force
Copy-Item -LiteralPath "C:\dev\helworks\helengine-psp\build\EBOOT.PBP" -Destination (Join-Path $targetRoot "EBOOT.PBP") -Force

rtk proxy powershell -File C:\dev\helworks\helengine-psp\tools\run_ppsspp_boot_check.ps1 | Select-String "PspPerfFrame3D|LoadStartupScene|gpuDirectionalCount|cpuFallbackCount"
'@
```

Expected: PASS with a `PspPerfFrame3D` line that now includes `gpuDirectionalCount=` and `cpuFallbackCount=`. Do not require a specific count yet.

- [ ] **Step 5: Commit the profiler evidence seam**

```bash
git add src/platform/psp/rendering/PspRenderProfiler.hpp src/platform/psp/rendering/PspRenderProfiler.cpp src/platform/psp/rendering/PspRenderManager3D.cpp
git commit -m "Add PSP GPU lighting path profiler counters"
```

### Task 2: Make the GPU Compatibility Slice Explicit

**Files:**
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`

- [ ] **Step 1: Add explicit renderer helpers for compatibility checks and CPU-fallback draw-state preparation**

In `src/platform/psp/rendering/PspRenderManager3D.hpp`, add these private declarations:

```cpp
        /// Returns whether one drawable matches the narrow PSP GPU directional-light compatibility slice.
        bool CanUseGpuDirectionalLighting(
            const class PspRuntimeModel* runtimeModel,
            class PspRuntimeMaterial* runtimeMaterial,
            bool hasTexture,
            bool useLighting) const;

        /// Prepares GU state for one CPU-lit fallback draw after any prior fixed-function GPU draw.
        void PrepareCpuFallbackDrawState(class PspRuntimeTexture* texture);

        /// Submits one textured drawable through the existing CPU-lit fallback path.
        void SubmitCpuLitTexturedDrawable(
            Entity* drawableParent,
            const class PspRuntimeModel* runtimeModel,
            const float4& baseColor,
            bool useLighting,
            class PspRuntimeTexture* texture);
```

- [ ] **Step 2: Implement the compatibility helper and the CPU-fallback draw-state reset**

In `src/platform/psp/rendering/PspRenderManager3D.cpp`, add the new methods:

```cpp
    bool PspRenderManager3D::CanUseGpuDirectionalLighting(
        const PspRuntimeModel* runtimeModel,
        PspRuntimeMaterial* runtimeMaterial,
        bool hasTexture,
        bool useLighting) const {
        return runtimeModel != nullptr
            && runtimeMaterial != nullptr
            && runtimeModel->HasFixedFunctionVertices()
            && !hasTexture
            && useLighting
            && CurrentLighting.HasDirectionalLight;
    }

    void PspRenderManager3D::PrepareCpuFallbackDrawState(PspRuntimeTexture* texture) {
        SetLight0Enabled(false);
        SetLightingEnabled(false);
        BindTexture(texture);
        sceGuColor(0xffffffff);
        sceGuAmbientColor(0xffffffff);
    }
```

- [ ] **Step 3: Move the inline textured CPU fallback into a dedicated helper**

Still in `src/platform/psp/rendering/PspRenderManager3D.cpp`, factor the existing inline textured CPU path into one method so `Visit(...)` can make the compatibility decision once:

```cpp
    void PspRenderManager3D::SubmitCpuLitTexturedDrawable(
        Entity* drawableParent,
        const PspRuntimeModel* runtimeModel,
        const float4& baseColor,
        bool useLighting,
        PspRuntimeTexture* texture) {
        if (runtimeModel == nullptr || !runtimeModel->HasFixedFunctionTexturedVertices()) {
            return;
        }

        int32_t vertexCount = runtimeModel->GetFixedFunctionTexturedVertexCount();
        if (vertexCount < 3) {
            return;
        } else if (texture == nullptr || !texture->HasPixels()) {
            throw std::runtime_error("Textured PSP drawables require mesh texcoords and a runtime texture.");
        }

        PrepareCpuFallbackDrawState(texture);

        PspTexturedLitVertex* vertices = static_cast<PspTexturedLitVertex*>(sceGuGetMemory(sizeof(PspTexturedLitVertex) * static_cast<std::size_t>(vertexCount)));
        const PspRuntimeModel::FixedFunctionTexturedVertex* sourceVertices = runtimeModel->GetFixedFunctionTexturedVertices();
        for (int32_t index = 0; index < vertexCount; index++) {
            const PspRuntimeModel::FixedFunctionTexturedVertex& sourceVertex = sourceVertices[index];
            const float3 position(sourceVertex.X, sourceVertex.Y, sourceVertex.Z);
            const float3 sourceNormal(sourceVertex.NX, sourceVertex.NY, sourceVertex.NZ);
            const float3 worldNormal = float3::Normalize(RotateNormal(sourceNormal, drawableParent));
            const float4 litColor = EvaluateCpuLitColor(baseColor, worldNormal, useLighting, LightingSettings, CurrentLighting);

            vertices[index] = PspTexturedLitVertex {
                sourceVertex.U,
                sourceVertex.V,
                ConvertColorToAbgr(litColor),
                position.X,
                position.Y,
                position.Z
            };
        }

        sceGumDrawArray(
            GU_TRIANGLES,
            GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
            vertexCount,
            nullptr,
            vertices);
    }
```

Also update the untextured CPU helper signature and its call site so it receives `Entity* drawableParent` directly instead of reaching through `drawable->get_Parent()` again:

```cpp
        void SubmitCpuLitDrawable(
            Entity* drawableParent,
            const PspRuntimeModel* runtimeModel,
            const float4& baseColor,
            bool useLighting,
            const PspLightingSettings& lightingSettings,
            const PspSceneLightingSnapshot& lightingSnapshot) {
            if (runtimeModel == nullptr || !runtimeModel->HasFixedFunctionVertices()) {
                return;
            }
```

- [ ] **Step 4: Rewrite `Visit(...)` to choose the GPU path only for the narrow supported slice**

Replace the current broad `FixedFunctionLambert` branch in `src/platform/psp/rendering/PspRenderManager3D.cpp` with this structure:

```cpp
        const bool canUseGpuDirectionalLighting = CanUseGpuDirectionalLighting(
            pspRuntimeModelData,
            pspRuntimeMaterial,
            hasTexture,
            useLighting);

        if (canUseGpuDirectionalLighting) {
            PspRenderProfiler::Record3DGpuDirectionalVisit();
            SubmitFixedFunctionDrawable(pspRuntimeModelData, baseColor, true);
            PspRenderProfiler::Record3DVisit(PspRenderProfiler::GetTimestampMicroseconds() - visitStartMicroseconds);
            return;
        }

        PspRenderProfiler::Record3DCpuFallbackVisit();
        if (hasTexture) {
            SubmitCpuLitTexturedDrawable(drawableParent, pspRuntimeModelData, baseColor, useLighting, texture);
            PspRenderProfiler::Record3DVisit(PspRenderProfiler::GetTimestampMicroseconds() - visitStartMicroseconds);
            return;
        }

        PrepareCpuFallbackDrawState(nullptr);
        SubmitCpuLitDrawable(drawableParent, pspRuntimeModelData, baseColor, useLighting, LightingSettings, CurrentLighting);
        PspRenderProfiler::Record3DVisit(PspRenderProfiler::GetTimestampMicroseconds() - visitStartMicroseconds);
```

This is the core milestone behavior:
- untextured lit directional meshes with one active directional light use GU fixed-function lighting
- everything else falls back to CPU lighting
- there are still no scene-name checks

- [ ] **Step 5: Rebuild the profiler-enabled PSP runtime and verify `cube_test` now chooses the GPU path explicitly**

Run:

```powershell
rtk proxy powershell -Command @'
$ErrorActionPreference = "Stop"

$buildConfigPath = "C:\dev\helprojs\city\user_settings\build_config.json"
$backupPath = "$buildConfigPath.gpu-lighting-plan.bak"
Copy-Item -LiteralPath $buildConfigPath -Destination $backupPath -Force

try {
    $buildConfig = Get-Content -LiteralPath $buildConfigPath -Raw | ConvertFrom-Json
    $pspPlatform = $buildConfig.platforms | Where-Object { $_.platformId -eq "psp" }
    if ($null -eq $pspPlatform) {
        throw "PSP platform configuration was not found."
    }

    $pspPlatform.sceneOrders = @(
        [pscustomobject]@{ sceneId = "cube_test"; orderNumber = 1 },
        [pscustomobject]@{ sceneId = "DemoDiscMainMenu"; orderNumber = 2 },
        [pscustomobject]@{ sceneId = "colored_cube_grid"; orderNumber = 3 },
        [pscustomobject]@{ sceneId = "textured_cube_grid"; orderNumber = 4 },
        [pscustomobject]@{ sceneId = "axis_test"; orderNumber = 5 },
        [pscustomobject]@{ sceneId = "axis_test2"; orderNumber = 6 },
        [pscustomobject]@{ sceneId = "directional_shadow_plaza"; orderNumber = 7 },
        [pscustomobject]@{ sceneId = "spotlight_street_slice"; orderNumber = 8 }
    )

    $buildConfig | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $buildConfigPath

    rtk dotnet "C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll" --project "C:\dev\helprojs\city\project.heproj" --build psp --output "C:\dev\helprojs\output\psp-cube-test-gpu-lighting"

    $workspaceRoot = Get-ChildItem "$env:LOCALAPPDATA\Temp\helengine-platform-build\psp" -Directory | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($null -eq $workspaceRoot) {
        throw "No PSP generated-core workspace was found."
    }

    $generatedCoreRoot = Join-Path $workspaceRoot.FullName "generated-core"
    rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v "${generatedCoreRoot}:/generated-core" -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON HELENGINE_PSP_ENABLE_BOOT_TRACE=ON HELENGINE_PSP_ENABLE_RENDER_PROFILER=ON

    $sourceRoot = "C:\dev\helprojs\output\psp-cube-test-gpu-lighting\PSP\GAME\HELENGINE"
    $targetRoot = "C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE"
    if (Test-Path -LiteralPath $targetRoot) {
        Remove-Item -LiteralPath $targetRoot -Recurse -Force
    }

    Copy-Item -LiteralPath $sourceRoot -Destination $targetRoot -Recurse -Force
    Copy-Item -LiteralPath "C:\dev\helworks\helengine-psp\build\EBOOT.PBP" -Destination (Join-Path $targetRoot "EBOOT.PBP") -Force

    rtk proxy powershell -File C:\dev\helworks\helengine-psp\tools\run_ppsspp_boot_check.ps1 | Select-String "PspPerfFrame3D|LoadStartupScene|gpuDirectionalCount|cpuFallbackCount"
} finally {
    if (Test-Path -LiteralPath $backupPath) {
        Move-Item -LiteralPath $backupPath -Destination $buildConfigPath -Force
    }
}
'@
```

Expected: PASS with steady `PspPerfFrame3D` lines showing:
- `LoadStartupScene id=cube_test`
- `gpuDirectionalCount=1`
- `cpuFallbackCount=0`

If `cpuFallbackCount` is non-zero on `cube_test`, stop and inspect the compatibility helper before widening scope.

- [ ] **Step 6: Commit the renderer routing change**

```bash
git add src/platform/psp/rendering/PspRenderManager3D.hpp src/platform/psp/rendering/PspRenderManager3D.cpp
git commit -m "Route PSP cube-test lighting through explicit GPU compatibility path"
```

### Task 3: Run the Regression and Smoke Verification Pass

**Files:**
- Modify temporarily only: `C:\dev\helprojs\city\user_settings\build_config.json`
- Produce temporarily only: `C:\dev\helprojs\output\psp-cube-test-gpu-lighting`
- Produce temporarily only: `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`

- [ ] **Step 1: Re-run the `cube_test` verification and capture the path-selection evidence for the final notes**

Run:

```powershell
rtk proxy powershell -File C:\dev\helworks\helengine-psp\tools\run_ppsspp_boot_check.ps1 | Select-String "PspPerfFrame3D|LoadStartupScene|gpuDirectionalCount|cpuFallbackCount"
```

Expected: PASS with `gpuDirectionalCount=1` and `cpuFallbackCount=0` for steady `cube_test` frames. Keep the emitted boot log as evidence only; do not commit it.

- [ ] **Step 2: Restore the PSP scene order to the normal demo-disc startup and export a smoke-test build**

Run:

```powershell
rtk proxy powershell -Command @'
$ErrorActionPreference = "Stop"

$buildConfigPath = "C:\dev\helprojs\city\user_settings\build_config.json"
$buildConfig = Get-Content -LiteralPath $buildConfigPath -Raw | ConvertFrom-Json
$pspPlatform = $buildConfig.platforms | Where-Object { $_.platformId -eq "psp" }
if ($null -eq $pspPlatform) {
    throw "PSP platform configuration was not found."
}

$pspPlatform.sceneOrders = @(
    [pscustomobject]@{ sceneId = "DemoDiscMainMenu"; orderNumber = 1 },
    [pscustomobject]@{ sceneId = "cube_test"; orderNumber = 2 },
    [pscustomobject]@{ sceneId = "colored_cube_grid"; orderNumber = 3 },
    [pscustomobject]@{ sceneId = "textured_cube_grid"; orderNumber = 4 },
    [pscustomobject]@{ sceneId = "axis_test"; orderNumber = 5 },
    [pscustomobject]@{ sceneId = "axis_test2"; orderNumber = 6 },
    [pscustomobject]@{ sceneId = "directional_shadow_plaza"; orderNumber = 7 },
    [pscustomobject]@{ sceneId = "spotlight_street_slice"; orderNumber = 8 }
)

$buildConfig | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $buildConfigPath
rtk dotnet "C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll" --project "C:\dev\helprojs\city\project.heproj" --build psp --output "C:\dev\helprojs\output\psp-cube-test-gpu-lighting-smoke"
'@
```

Expected: PASS with a fresh export at `C:\dev\helprojs\output\psp-cube-test-gpu-lighting-smoke\PSP\GAME\HELENGINE`.

- [ ] **Step 3: Rebuild the native PSP runtime with boot trace and profiler enabled, then stage it with the smoke-test cooked assets**

Run:

```powershell
rtk proxy powershell -Command @'
$ErrorActionPreference = "Stop"

$workspaceRoot = Get-ChildItem "$env:LOCALAPPDATA\Temp\helengine-platform-build\psp" -Directory | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if ($null -eq $workspaceRoot) {
    throw "No PSP generated-core workspace was found."
}

$generatedCoreRoot = Join-Path $workspaceRoot.FullName "generated-core"
rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v "${generatedCoreRoot}:/generated-core" -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON HELENGINE_PSP_ENABLE_BOOT_TRACE=ON HELENGINE_PSP_ENABLE_RENDER_PROFILER=ON

$sourceRoot = "C:\dev\helprojs\output\psp-cube-test-gpu-lighting-smoke\PSP\GAME\HELENGINE"
$targetRoot = "C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE"
if (Test-Path -LiteralPath $targetRoot) {
    Remove-Item -LiteralPath $targetRoot -Recurse -Force
}

Copy-Item -LiteralPath $sourceRoot -Destination $targetRoot -Recurse -Force
Copy-Item -LiteralPath "C:\dev\helworks\helengine-psp\build\EBOOT.PBP" -Destination (Join-Path $targetRoot "EBOOT.PBP") -Force
'@
```

Expected: PASS with the demo-disc cooked assets staged into the PPSSPP memstick and the rebuilt `EBOOT.PBP` copied over them.

- [ ] **Step 4: Run the PPSSPP smoke boot and confirm unsupported scene startup still reaches the menu**

Run:

```powershell
rtk proxy powershell -File C:\dev\helworks\helengine-psp\tools\run_ppsspp_boot_check.ps1 | Select-String "LoadStartupScene|RuntimeMainLoop|PspPerfFrame3D"
```

Expected: PASS with:
- `LoadStartupScene id=DemoDiscMainMenu`
- later startup progress reaching `RuntimeMainLoop`
- a `PspPerfFrame3D` line, proving the build still boots with unsupported scenes available behind the menu

- [ ] **Step 5: Do not commit temporary build-config or output artifacts**

Run:

```bash
git status --short
```

Expected: only the source files committed in Tasks 1 and 2 should remain as tracked history. Do not add:
- `C:\dev\helprojs\city\user_settings\build_config.json`
- any `C:\dev\helprojs\output\...` directory
- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`

## Self-Review

1. **Spec coverage:** The plan covers explicit GPU submission for the narrow compatibility slice in Task 2, explicit CPU fallback in Task 2, path-selection verification in Task 1 and Task 3, and unsupported-scene smoke verification in Task 3. The plan intentionally does not widen into textured GPU lighting, spot lights, point lights, shadows, or multi-material rendering.
2. **Placeholder scan:** There are no `TODO`, `TBD`, or “similar to Task N” instructions. Every task names exact files, concrete code snippets, and exact commands.
3. **Type consistency:** The plan uses one consistent naming set across all tasks:
   - `CanUseGpuDirectionalLighting(...)`
   - `PrepareCpuFallbackDrawState(...)`
   - `SubmitCpuLitTexturedDrawable(...)`
   - `Record3DGpuDirectionalVisit()`
   - `Record3DCpuFallbackVisit()`
