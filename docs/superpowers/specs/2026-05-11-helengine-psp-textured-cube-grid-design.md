# PSP Textured Cube Grid Design

## Goal

Render the real `textured_cube_grid` city scene on PSP in PPSSPP with:

- distinct authored textures per cube
- directional lighting active at the same time
- no fallback diagnostic materials or fake texture paths

This milestone should extend the current working PSP directional-lighting path instead of replacing it.

## Scope

In scope:

- PSP runtime support for textured materials
- PSP runtime support for mesh UVs
- PSP render submission that combines texture sampling with the existing CPU directional-lighting path
- full-scene verification on the authored `textured_cube_grid` scene

Out of scope for this milestone:

- mipmaps
- filtered sampling beyond nearest
- specular, emissive, point lights, or spot lights
- a fixed-function lighting rewrite
- PSP-only authored material or scene semantics

## Existing Context

The current PSP renderer already reaches the real scene main loop, renders the colored cube grid, and applies directional lighting on CPU. The city project already contains the authored `textured_cube_grid` scene and its distinct textured materials. The missing capability is PSP texture handling inside the runtime material and renderer path.

## Recommended Approach

Use CPU-lit textured vertices with GU texture sampling.

Why:

- directional lighting is already working on CPU and should stay stable for this milestone
- texture support can be added as an orthogonal material/runtime capability
- this keeps the authored material contract portable and allows a later fixed-function backend without changing cooked scene semantics

Rejected for this milestone:

- full fixed-function PSP lighting plus texturing in one step: too much architecture churn at once
- unlit textured-only first: lower risk, but it does not meet the requested milestone

## Rendering Model

The first textured PSP shading model is:

`finalColor = sampledTexture * baseColor * (ambient + directionalDiffuse)`

Behavior:

- untextured materials continue to use the current directional-lighting path
- textured materials modulate sampled texels with the CPU-computed lit vertex color
- ambient remains the PSP renderer default already established for directional lighting
- directional light still comes from live scene components

## Material Contract

The PSP material contract remains renderer-agnostic:

- `base-color`
- `lighting-response`
- `receives-lighting`
- diffuse texture binding when present

Requirements:

- authored textured materials must not need PSP-specific scene changes
- PSP runtime decides whether a diffuse texture exists and how it is uploaded
- base color remains active for both textured and untextured materials

## Cook And Asset Strategy

Cook for the common portable contract first, specialize at runtime.

Requirements:

- cooked mesh data must retain UVs alongside positions, normals, and indices
- cooked material data must preserve diffuse texture binding for PSP runtime consumption
- cooked texture assets must be staged into the PSP game folder for the startup scene

Design rule:

- do not introduce a PSP-only authored texture format for this milestone
- if PSP needs an upload adapter or decode shim, add that at runtime first
- only introduce PSP-specific cook divergence later if profiling proves it is necessary

## Runtime Architecture

### PSP Runtime Material

Extend PSP runtime materials to carry:

- existing base color
- existing lighting flags
- diffuse texture presence/binding
- cached PSP texture resource handle or runtime-owned texture object

### PSP Runtime Mesh

Extend PSP runtime mesh storage to preserve UVs in addition to:

- positions
- normals
- indices

### PSP Texture Cache

Add a PSP runtime texture cache keyed by cooked texture identity or packaged asset path.

Responsibilities:

- load texture data once
- decode or adapt cooked texture payloads as needed for PSP upload
- upload PSP texture memory/state once per texture
- reuse across drawables and frames

### Renderer Submission

For textured drawables, PSP render submission should:

1. resolve the runtime material and its texture
2. resolve the runtime mesh including UVs
3. compute directional lighting on CPU per vertex
4. emit GU vertices containing:
   - UV
   - lit color
   - position
5. bind the GU texture and render with texture modulation enabled

This keeps lighting and texturing separable:

- CPU lighting remains the current implementation path
- GU texture sampling becomes the new material path
- fixed-function lighting can later replace only the lighting evaluator/backend layer

## Sampling Rules

The first milestone sampling bar is intentionally minimal:

- nearest sampling
- no mipmaps

This is acceptable for the milestone as long as:

- textures are visibly correct
- cubes remain distinct
- directional lighting still shapes the scene

## Success Criteria

The milestone is complete when:

- the real `textured_cube_grid` scene boots on PSP
- cubes rotate as expected
- each cube shows its authored distinct texture
- directional lighting remains active across the scene
- the result is not a flat unlit texture pass and not a diagnostic fallback

## Failure Boundaries To Debug

Expected likely failure points:

- startup-scene materialization fails on textured material references
- cooked textures are not staged into the PSP app folder
- runtime texture decode/upload fails
- UV data is missing or mismatched
- GU state is wrong and textures render white, black, or scrambled
- texture modulation overrides lighting or lighting overrides texture

Debug rule:

- stop at the first real failing boundary and fix that boundary directly
- do not add fallback rendering modes to hide broken texture/material contracts

## Verification Plan

Required verification:

- PSP builder tests pass
- targeted regressions cover texture-binding preservation where feasible
- PSP native rebuild passes
- fresh city PSP export succeeds
- PPSSPP boots the real `textured_cube_grid` scene
- on-screen output shows distinct textures under directional lighting

## Risks

- current cooked texture payloads may need a PSP runtime decode step before upload
- texture staging may be incomplete even if material references are correct
- GU vertex layout changes may introduce regressions in existing untextured rendering if not isolated cleanly

## Recommendation

Implement this as one vertical slice ending at the real `textured_cube_grid` scene in PPSSPP. Keep the authored asset contract stable, keep lighting on CPU for now, and add GU texture sampling as the new runtime material capability.
