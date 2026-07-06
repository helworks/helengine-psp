# Helengine PSP Physics Performance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce PSP physics CPU cost by applying a PSP-only low-precision runtime preset of `30 Hz`, `2` solve velocity iterations, `1` substep, and `2` maximum catch-up steps per update.

**Architecture:** Keep all tuning local to the PSP native host so shared BEPU defaults and non-PSP platforms stay unchanged. Replace the generic PSP physics registration call with an explicit PSP runtime world construction path in `PspBootHost`, while also overriding the core physics scheduler settings before core initialization.

**Tech Stack:** .NET 9, xUnit, C++20, PSPDEV/PSPSDK, PPSSPP, generated native core, BEPU-backed 3D physics runtime

---

## File Structure

- Modify: `builder.tests/PspPackagedRuntimeSourceTests.cs`
  Purpose: lock the PSP host source contract so physics tuning stays explicit and does not regress back to the default desktop-oriented path.
- Modify: `src/platform/psp/PspBootHost.cpp`
  Purpose: apply the PSP-only scheduler overrides and register a cheaper PSP-specific BEPU physics world after core initialization.
- Modify: `src/platform/psp/PspBootHost.hpp`
  Purpose: only if needed to declare any new helper methods for PSP-specific physics registration.
- Modify: `docs/superpowers/specs/2026-06-29-psp-physics-performance-design.md`
  Purpose: only if implementation reveals one small design correction that must be recorded.

### Task 1: Add The Failing PSP Physics Tuning Source Test

**Files:**
- Modify: `builder.tests/PspPackagedRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing source test**

Add one test beside the existing `PspBootHost` source assertions that reads `src/platform/psp/PspBootHost.cpp` and asserts all of the following strings exist:

```csharp
Assert.Contains("EngineOptions->set_PhysicsFixedStepSeconds(1.0 / 30.0);", source, StringComparison.Ordinal);
Assert.Contains("EngineOptions->set_PhysicsMaxStepsPerUpdate(2);", source, StringComparison.Ordinal);
Assert.Contains("BepuPhysicsWorld3D::CreateWithSolveSchedule(2, 1)", source, StringComparison.Ordinal);
Assert.Contains("BepuRuntimeComponentRegistration::AttachRuntimeWorld(EngineCore, physicsWorld);", source, StringComparison.Ordinal);
Assert.Contains("BepuRuntimeComponentRegistration::RegisterSceneBinding(EngineCore);", source, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the targeted test and verify it fails**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-psp\builder.tests\helengine.psp.builder.tests.csproj --filter PspBootHost_uses_low_precision_psp_physics_runtime_tuning
```

Expected: `FAIL` because `PspBootHost.cpp` still uses the default physics scheduler values and the generic runtime registration path.

- [ ] **Step 3: Commit the failing-test checkpoint**

```powershell
git -C C:\dev\helworks\helengine-psp add -- builder.tests\PspPackagedRuntimeSourceTests.cs
git -C C:\dev\helworks\helengine-psp commit -m "test: cover PSP low-precision physics tuning"
```

### Task 2: Apply PSP-Only Physics Scheduler And Solver Tuning

**Files:**
- Modify: `src/platform/psp/PspBootHost.cpp`
- Modify: `src/platform/psp/PspBootHost.hpp` if a helper declaration is needed

- [ ] **Step 1: Add the required generated-core physics headers**

In `src/platform/psp/PspBootHost.cpp`, add the includes needed for the explicit PSP-specific runtime registration path:

```cpp
#include "BepuPhysicsWorld3D.hpp"
#include "BepuRuntimeComponentRegistration.hpp"
```

Keep the existing `#include "Physics3DRuntimeComponentRegistration.hpp"` only if still needed after the new explicit path; otherwise remove it.

- [ ] **Step 2: Override the PSP core physics scheduler settings before core initialization**

In `PspBootHost::InitializeCore`, after the existing `EngineOptions` setup and before `EngineCore->Initialize(...)`, add:

```cpp
        EngineOptions->set_PhysicsFixedStepSeconds(1.0 / 30.0);
        EngineOptions->set_PhysicsMaxStepsPerUpdate(2);
```

Do not change unrelated core initialization options.

- [ ] **Step 3: Replace the generic PSP physics registration call with an explicit cheap BEPU world**

Immediately after `EngineCore->Initialize(...)`, replace the generic runtime registration path with:

```cpp
        BepuPhysicsWorld3D* physicsWorld = BepuPhysicsWorld3D::CreateWithSolveSchedule(2, 1);
        BepuRuntimeComponentRegistration::AttachRuntimeWorld(EngineCore, physicsWorld);
        BepuRuntimeComponentRegistration::RegisterSceneBinding(EngineCore);
```

This keeps scene binding behavior intact while swapping the expensive default world construction for the PSP-specific solve schedule.

- [ ] **Step 4: Keep the host code readable**

If the new registration block is hard to scan inline, add one short private helper on `PspBootHost` and declare it in `PspBootHost.hpp`:

```cpp
        void RegisterPhysicsRuntime();
```

Only do this if it improves readability without spreading the change across extra files.

- [ ] **Step 5: Run the targeted PSP source test and verify it passes**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-psp\builder.tests\helengine.psp.builder.tests.csproj --filter PspBootHost_uses_low_precision_psp_physics_runtime_tuning
```

Expected: `PASS`.

- [ ] **Step 6: Commit the PSP tuning implementation**

```powershell
git -C C:\dev\helworks\helengine-psp add -- src\platform\psp\PspBootHost.cpp src\platform\psp\PspBootHost.hpp builder.tests\PspPackagedRuntimeSourceTests.cs
git -C C:\dev\helworks\helengine-psp commit -m "feat: tune PSP physics runtime for performance"
```

### Task 3: Rebuild And Boot The Tuned PSP Runtime

**Files:**
- No source changes required unless verification reveals a regression

- [ ] **Step 1: Rebuild the PSP artifact**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\artifacts\build-platform.ps1 -Project C:\dev\helprojs\city\project.heproj -Platform psp -Output C:\dev\helprojs\city\psp-build
```

Expected: build completes successfully and emits `C:\dev\helprojs\city\psp-build\PSP\GAME\HELENGINE\EBOOT.PBP`.

- [ ] **Step 2: Relaunch the new PSP artifact in PPSSPP**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-psp\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\city\psp-build\PSP\GAME\HELENGINE\EBOOT.PBP
```

Expected: the artifact installs into the PPSSPP `HELENGINE` app folder and starts the emulator process.

- [ ] **Step 3: Record the runtime check**

Verify in PPSSPP that:

- `Stacked Boxes` still simulates
- the scene is materially more responsive than the prior `60 Hz / 4 iteration` PSP path
- reduced precision is acceptable for this first pass

- [ ] **Step 4: Commit only if follow-up source changes were needed during verification**

```powershell
git -C C:\dev\helworks\helengine-psp add -- <files changed during verification>
git -C C:\dev\helworks\helengine-psp commit -m "fix: finalize PSP physics performance tuning"
```
