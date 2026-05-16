# PSP Textured Directional GPU Lighting Design

## Goal

Widen the PSP fixed-function GPU lighting path so any compatible textured directional-lit scene can use it, not just untextured scenes like `cube_test` and `colored_cube_grid`.

The immediate validation target is `textured_cube_grid`, but the implementation must stay engine-level. If a PSP drawable has the right mesh data, a resolved runtime texture, and the current directional-light material shape, it should route through the GPU path automatically.

## Current State

The PSP renderer already has:

- a narrow GPU directional-light path for untextured fixed-function meshes
- a fixed-function textured draw path for unlit or non-GPU-routed textured submission
- a CPU-lit textured fallback path that bakes per-vertex lighting on the CPU before GU submission

Today, textured directional-lit drawables are explicitly excluded from GPU routing in `CanUseGpuDirectionalLighting(...)` because the gate requires `!hasTexture`.

That means:

- `cube_test` uses GPU directional lighting
- `colored_cube_grid` uses GPU directional lighting
- `textured_cube_grid` still uses CPU-lit textured fallback

## Requirements

The new PSP GPU path must:

- support textured directional-lit meshes for any compatible scene
- preserve authored base color tinting
- preserve resolved PSP runtime textures
- keep CPU fallback intact for incompatible cases
- avoid scene-specific branches

This pass does not include:

- spot lights
- point lights
- shadows
- multi-light blending
- batching changes

## Compatibility Rules

A drawable can use textured PSP GPU directional lighting only when all of these are true:

- it has a valid `PspRuntimeModel`
- the model has fixed-function textured vertices
- the material resolves a valid `PspRuntimeTexture`
- the material lighting mode is directional-lit
- the material receives lighting
- the current scene lighting snapshot has an active directional light

If any condition fails, the renderer must continue using the current CPU fallback path.

## Rendering Design

The PSP renderer already owns the right primitives for this work:

- `SubmitFixedFunctionTexturedDrawable(...)`
- `ConfigureFixedFunctionMaterial(...)`
- `BindTexture(...)`
- `ConfigureFixedFunctionSceneLighting()`

The design is to widen routing, not to invent a separate textured-only renderer.

### GPU Routing

`CanUseGpuDirectionalLighting(...)` should stop excluding textured drawables purely because they have a texture. Instead, it should accept either:

- untextured fixed-function vertices, or
- textured fixed-function vertices with a valid resolved runtime texture

That keeps one compatibility decision point for both untextured and textured GPU directional lighting.

### Draw Submission

When the GPU directional path is selected:

- untextured drawables continue using `SubmitFixedFunctionDrawable(...)`
- textured drawables use `SubmitFixedFunctionTexturedDrawable(...)`

No CPU-lit textured vertex expansion should happen on that path.

### Material Behavior

Authored `base-color` must continue to tint textured GPU-lit materials through `ConfigureFixedFunctionMaterial(...)`.

The GPU path should keep using:

- `sceGuColor(...)`
- `sceGuAmbientColor(...)`
- `sceGuModelColor(...)`

so authored tint remains consistent with the current untextured GPU path and with the intended shared material contract.

### Texture Behavior

Textured GPU-lit drawables should keep using the existing texture binding flow:

- resolve `PspRuntimeTexture`
- `BindTexture(texture)`
- draw with `GU_TEXTURE_32BITF | GU_NORMAL_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D`

This is already how `SubmitFixedFunctionTexturedDrawable(...)` submits vertices, so the main work is routing compatible drawables into it.

## Fallback Behavior

The CPU fallback remains the safety net.

Textured drawables must keep using CPU fallback when:

- there is no resolved PSP runtime texture
- the mesh lacks fixed-function textured vertices
- the material does not use the directional-light response
- scene lighting is absent

That means the renderer still supports mixed scenes safely while we expand GPU coverage incrementally.

## Profiling and Verification

The existing profiler counters remain the verification source:

- `gpuDirectionalCount`
- `cpuFallbackCount`

For `textured_cube_grid`, expected verification after the change is:

- `gpuDirectionalCount` rises to the visible textured cube draw count
- `cpuFallbackCount` drops accordingly, ideally to zero for the scene

Visual verification must confirm:

- textured cubes still show distinct textures
- directional lighting still reacts correctly
- authored material tint stays correct

## Risks

The main risks are:

- textured GPU-lit materials losing tint because GU material color state differs from the CPU-lit path
- textured meshes with incomplete texcoord data being routed incorrectly
- accidental regressions in the existing untextured GPU path

The design contains these risks by:

- keeping one compatibility gate
- preserving the current textured CPU fallback
- reusing the existing fixed-function textured submission path instead of adding a second textured GPU implementation

## Success Criteria

This pass is successful when:

- `textured_cube_grid` runs through the GPU directional path on PSP
- compatible textured directional-lit scenes also route automatically without scene-specific logic
- incompatible scenes still fall back correctly
- no generated-code patching is required
