# PSP Colored Cube Grid GPU Color Preservation

## Goal

Extend the current PSP GPU directional-lighting path so `colored_cube_grid` preserves the authored per-cube base colors while remaining fully GPU lit.

This pass is intentionally about correctness, not speed. The user only wants to prove that the GPU path can handle repeated untextured directional-lit draws with different material colors without falling back to CPU lighting or collapsing everything to one color.

## Problem

`cube_test` already proves that the new PSP GPU directional-light path can light one compatible untextured mesh on the GPU.

`colored_cube_grid` raises the next compatibility requirement:

- `16` rotating cubes
- same generated cube model
- same directional-light setup
- same untextured lit material intent
- different authored `base-color` on each cube

The renderer now needs to show that GPU lighting can preserve those per-draw material colors across repeated draws, not just light one cube correctly.

## Scope

This pass only widens the current PSP GPU path to the next compatible case:

- untextured fixed-function directional lighting with distinct per-material base colors across repeated draws

This pass does not include:

- batching
- performance tuning
- textured GPU lighting
- spot lights
- point lights
- shadows
- specular
- submesh or material-slot fixes
- scene-specific code

## Compatibility Slice

The GPU path should activate only when all of these are true:

- the runtime model exposes PSP-ready fixed-function vertex data
- the material is untextured
- the material receives lighting
- the material lighting response is directional-lit
- the active scene lighting contains the supported directional-light configuration

If any condition fails, the renderer must continue using the current fallback path.

## Renderer Design

### 1. Keep the current GPU path selection model

The renderer should continue deciding per drawable whether the GPU directional path is valid.

This pass should not add a new scene-specific switch for `colored_cube_grid`. The only change is that the existing compatible path must remain correct when invoked repeatedly with different material colors.

### 2. Treat base color as per-draw fixed-function material state

The authored `base-color` must be applied at draw time through PSP GU material/color state, not by mutating shared mesh data and not by baking per-scene color assumptions into the renderer.

For this compatibility slice:

- the model data stays shared
- the light stays shared
- the per-draw material color is the only changing artistic input

That color must stay isolated to the current draw and must not leak into the next cube.

### 3. Preserve current fallback behavior

If the GPU path cannot express a drawable correctly, the renderer must still fall back cleanly to the CPU-lit path.

This milestone does not remove the fallback. It only widens confidence in the GPU path by proving one more compatible case.

### 4. Keep model and material ownership unchanged

The PSP runtime model should continue owning platform-ready fixed-function vertex streams.

The PSP runtime material should continue owning:

- base color
- receives-lighting
- lighting response

No raw-geometry ownership should move back into the renderer, and no authoring schema changes should be required.

## Expected Runtime Behavior

When `colored_cube_grid` is used as startup:

- all `16` cubes should render through the GPU directional path
- each cube should keep its authored color
- directional lighting should still respond consistently while the cubes rotate
- the renderer should report GPU-path usage rather than CPU fallback usage

The acceptable result is not "close enough" if colors collapse or smear between draws. Distinct per-cube colors are the point of this milestone.

## Verification

Verification should be done with `colored_cube_grid` as the active startup scene and should confirm both routing and visuals.

Required checks:

1. the startup manifest points to `colored_cube_grid`
2. PPSSPP reaches `RuntimeMainLoop`
3. profiler output shows GPU directional visits for the cube draws
4. CPU fallback count remains `0` for the compatible cubes
5. the user confirms the cubes keep distinct authored colors while rotating

## Risks

### GU state leakage

The most likely defect is stale GU material or color state carrying from one cube draw into the next. This is the primary thing this pass is meant to expose and fix.

### Hidden CPU fallback

The scene may appear "correct" while still silently using the CPU path. Verification must explicitly confirm GPU-path selection.

### Scope drift

It will be tempting to chase performance once the colors render correctly. That is explicitly out of scope for this pass.

## Success Criteria

This milestone is complete when all of these are true:

- `colored_cube_grid` renders through the PSP GPU directional path
- the `16` cubes preserve their distinct authored base colors
- GPU routing is confirmed in the profiler output
- no scene-specific hacks are introduced
- fallback behavior remains intact for unsupported drawables

## Follow-Up

Once this milestone is stable, the next GPU-lighting expansions should be:

1. performance measurement of the multi-draw GPU path versus CPU fallback
2. textured directional GPU lighting
3. broader lighting-model support
