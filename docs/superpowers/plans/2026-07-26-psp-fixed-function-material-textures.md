# PSP Fixed-Function Material Textures Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bind a PSP cooked material's referenced texture to its shaderless fixed-function runtime material.

**Architecture:** `PlatformMaterialAsset.TextureRelativePath` already stores the imported texture asset id. The PSP material load overload will resolve that id to `cooked/imported/<id>`, ask the existing PSP 2D texture manager to build the cached runtime texture, and assign it to `PspRuntimeMaterial`. Material release will release that same texture through the 2D manager.

**Tech Stack:** C++17, PSP GU fixed-function rendering, xUnit source-contract tests.

---

### Task 1: Specify material texture loading and release

**Files:**
- Modify: `builder.tests/PspPackagedRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing test**

Add an xUnit source-contract test asserting that the material cooked-load overload resolves `cooked/imported/` plus `materialAsset->TextureRelativePath`, calls `RenderManager2D->BuildTextureFromCooked`, and assigns the result through `SetPrimaryTexture`.

- [ ] **Step 2: Run test to verify it fails**

Run: `rtk dotnet test .\builder.tests\helengine.psp.builder.tests.csproj --no-restore --filter "FullyQualifiedName~PspRenderManager3D_loads_and_binds_the_cooked_material_texture"`

Expected: FAIL because the PSP renderer currently only deserializes the material payload.

### Task 2: Load and bind the fixed-function texture

**Files:**
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`
- Modify: `src/platform/psp/rendering/PspRuntimeMaterial.hpp`
- Modify: `src/platform/psp/rendering/PspRuntimeMaterial.cpp`

- [ ] **Step 1: Add the smallest material-owned texture reference**

Make `PspRuntimeMaterial` retain the `RuntimeTexture*` it receives through the existing primary-texture API so the renderer can release exactly the texture it loaded.

- [ ] **Step 2: Resolve the cooked imported texture during material loading**

In `PspRenderManager3D::BuildMaterialFromCooked(std::string, IContentStreamSource*)`, after deserializing the `PlatformMaterialAsset`, load `cooked/imported/` plus its non-empty `TextureRelativePath` using `RenderManager2D->BuildTextureFromCooked` and assign it to the newly-built material.

- [ ] **Step 3: Release material-owned textures through the 2D manager**

In `PspRenderManager3D::ReleaseMaterial`, release the retained material texture through `RenderManager2D->ReleaseTexture` before disposing the material.

- [ ] **Step 4: Run the focused test**

Run: `rtk dotnet test .\builder.tests\helengine.psp.builder.tests.csproj --no-restore --filter "FullyQualifiedName~PspRenderManager3D_loads_and_binds_the_cooked_material_texture"`

Expected: PASS.

### Task 3: Build and runtime-check

**Files:**
- No additional files.

- [ ] **Step 1: Run PSP source-contract tests**

Run: `rtk dotnet test .\builder.tests\helengine.psp.builder.tests.csproj --no-restore --filter "FullyQualifiedName~Psp"`

- [ ] **Step 2: Package the DemoDisc PSP build**

Run: `rtk powershell -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\helworks\helengine\scripts\build-platform.ps1' -Project 'C:\dev\helprojs\demodisc\project.heproj' -Platform psp -Output 'C:\dev\helprojs\output\psp-tilt-update-lifecycle'`

- [ ] **Step 3: Launch the produced PBP in PPSSPP**

Run: `rtk powershell -NoProfile -ExecutionPolicy Bypass -File '.\scripts\launch_in_emulator.ps1' -ArtifactPath 'C:\dev\helprojs\output\psp-tilt-update-lifecycle\PSP\GAME\HELENGINE\EBOOT.PBP'`

Expected: Tilt Play Level 01 loads with the authored lilac-grid texture on walls and ground.
