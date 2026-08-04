# PSP Baked Mesh Scale Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow a PSP MeshComponent to bake static non-uniform scale into its cooked model variant before optional tessellation, eliminating the PSP renderer's per-frame vertex scaling.

**Architecture:** Extend the existing editor-only `MeshComponentTessellationSettings` metadata with a PSP `Bake Scale` boolean and reuse its existing deterministic model-variant flow. Package a synthetic boolean on the MeshComponent so PSP runtime rendering can omit scale from the model matrix while physics retains the authored entity transform.

**Tech Stack:** C# editor packaging and binary serialization, generated C++ runtime schema, PSP GU fixed-function renderer, xUnit source and behavior tests.

---

### Task 1: Persist the Bake Scale platform setting

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\managers\scene\MeshComponentTessellationSettings.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\managers\scene\MeshComponentTessellationSettingsService.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\scene\MeshComponentTessellationSettingsServiceTests.cs`

- [ ] **Step 1: Write failing service tests**

Add a PSP test that stores `new MeshComponentTessellationSettings(true, 0.5d, true)`, then asserts `GetForPlatform(..., "psp")` returns `Tessellate == true`, `TessellationMaxEdgeLength == 0.5d`, and `BakeScale == true`. Add a second test asserting missing metadata returns `BakeScale == false`.

- [ ] **Step 2: Run the focused test and verify it fails**

Run: `rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --no-restore --filter FullyQualifiedName~MeshComponentTessellationSettingsServiceTests`

Expected: compile failure because the constructor and `BakeScale` property do not exist.

- [ ] **Step 3: Implement persisted metadata**

Add `bool BakeScale` to `MeshComponentTessellationSettings`, default it to `false`, and add the constant `MeshBakeScaleMemberName = "MeshBakeScale"` to its service. Persist the value using invariant boolean text and read missing values as `false`. Include `BakeScale` in `BuildVariantIdentity` so models with baked and non-baked scale never share an artifact.

- [ ] **Step 4: Re-run the focused test and commit**

Run the command from Step 2; expected result: all selected tests pass. Commit only the three files with message `Add PSP mesh bake-scale metadata`.

### Task 2: Bake model geometry before tessellation

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\managers\asset\ModelTessellationProcessor.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\managers\project\SceneComponentPackagingTransformService.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\ModelTessellationProcessorTests.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\SceneComponentPackagingTransformServiceTests.cs`

- [ ] **Step 1: Write failing geometry tests**

Add a test for a model with position `(1, 2, 3)` and normal `(1, 1, 0)` baked with scale `(2, 4, 1)`. Assert its position is `(2, 8, 3)` and its normal is normalized inverse-scale `(0.8944272, 0.4472136, 0)`. Add a packaging test with Bake Scale and tessellation enabled that asserts the generated model variant has scaled positions and is tessellated against final-scale edge lengths.

- [ ] **Step 2: Run the focused tests and verify they fail**

Run: `rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --no-restore --filter "FullyQualifiedName~ModelTessellationProcessorTests|FullyQualifiedName~SceneComponentPackagingTransformServiceTests"`

Expected: failure because no bake operation exists and packaging only uses scale for tessellation measurement.

- [ ] **Step 3: Implement one explicit geometry bake operation**

Add `ModelTessellationProcessor.ApplyBakeScale(ModelAsset asset, float3 scale)`. Validate finite nonzero scale, multiply positions component-wise, transform normals by reciprocal scale, and normalize with double precision. In `ApplyMeshComponentTessellationVariant`, invoke this operation first when `settings.BakeScale`; then invoke existing tessellation with `float3.One` when enabled. Keep the existing measurement-scale path for non-baked tessellation.

- [ ] **Step 4: Re-run focused tests and commit**

Run the command from Step 2; expected result: all selected tests pass. Commit only these four files with message `Bake PSP mesh scale before tessellation`.

### Task 3: Carry the baked-scale state into PSP runtime rendering

**Files:**
- Modify: `C:\dev\helworks\helengine-psp\builder\PspPlatformDefinitionFactory.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\managers\project\SceneComponentPackagingTransformService.cs`
- Modify: `C:\dev\helworks\helengine-psp\src\platform\psp\rendering\PspRenderManager3D.cpp`
- Modify: `C:\dev\helworks\helengine-psp\builder.tests\PspRenderManager3DSourceTests.cs`

- [ ] **Step 1: Write failing PSP source/packaging tests**

Add a source test requiring the PSP MeshComponent synthetic boolean member `MeshBakeScale` and requiring `PspRenderManager3D::Visit` to choose `BuildWorldMatrixWithoutScale` when it is true. Add packaging coverage asserting a baked PSP MeshComponent serializes `MeshBakeScale=true` while the entity transform scale remains authored.

- [ ] **Step 2: Run focused tests and verify they fail**

Run: `rtk dotnet test builder.tests\helengine.psp.builder.tests.csproj --no-restore --filter FullyQualifiedName~PspRenderManager3DSourceTests`

Expected: failure because the platform definition has no synthetic member and the renderer always applies non-uniform scale.

- [ ] **Step 3: Implement the synthetic render-only marker**

Declare `MeshBakeScale` as a boolean synthetic MeshComponent member in the PSP platform definition. Its name matches the persisted editor-only setting, so packaging writes the same override value into the runtime payload. In `PspRenderManager3D::Visit`, read the marker from the drawable component; for a marked binding, build the world matrix without scale and pass no position-scale buffer to fixed-function submission. Leave the entity transform itself unchanged so physics receives its authored scale.

- [ ] **Step 4: Re-run focused tests and commit**

Run the command from Step 2 and the packaging test filter from Task 2. Expected result: all selected tests pass. Commit only the PSP and packaging files with message `Render PSP baked-scale mesh variants without scale`.

### Task 4: Build and hardware-profile Tilt Play 01

**Files:**
- No source changes.

- [ ] **Step 1: Rebuild the PSP external builder DLL**

Run: `rtk dotnet build C:\dev\helworks\helengine-psp\builder\helengine.psp.builder.csproj --no-restore -c Debug`

Expected: exit code 0.

- [ ] **Step 2: Build the level-01 PSP renderer-profile package**

Run the normal `C:\dev\helworks\helengine\scripts\build-platform.ps1` PSP build script, preserving the level-01 selection and renderer profiler flags.

- [ ] **Step 3: Verify packaged profiler configuration**

Run: `rtk rg -n 'HELENGINE_PSP_ENABLE_(BOOT_TRACE|RENDER_PROFILER)' C:\dev\helworks\helengine-psp\build\CMakeCache.txt`

Expected: both values are `ON`.

- [ ] **Step 4: Hardware validation**

Run Tilt Play 01 on PSP hardware and collect `helengine_psp_boot.log`. Compare `PspPerfFrame3D` queue/visit timing with the current 110–114 ms baseline while confirming ground and walls remain textured and lit.
