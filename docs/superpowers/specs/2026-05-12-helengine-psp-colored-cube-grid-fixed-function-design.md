# PSP Colored Cube Grid Fixed-Function Design

## Goal

Make `colored_cube_grid` render through the current PSP fixed-function lighting path and validate that repeated untextured draws preserve authored base colors plus directional lighting across the full scene.

## Scope

- Keep `FixedFunctionLambert` forced as the active PSP lighting pipeline.
- Validate only `colored_cube_grid`.
- Support only untextured, opaque, solid-color materials in this milestone.
- Reuse the existing authored material semantics:
  - `base-color`
  - `receives-lighting`
  - `lit-directional`

## Out Of Scope

- Texture support in the fixed-function path
- Alpha blending or transparency
- CPU versus fixed-function comparison mode
- Point lights
- Spot lights
- Specular
- Emissive
- Shadows

## Architecture

The renderer contract stays semantic and unchanged at the asset level. The authored scene continues to describe only material color and lighting intent. The PSP runtime remains responsible for choosing the execution backend, and this milestone keeps that choice pinned to `FixedFunctionLambert`.

`colored_cube_grid` becomes the first multi-instance fixed-function verification scene. `cube_test` already proved that the fixed-function path can light one cube. This scene extends that proof to many independent draw calls with many material color changes while keeping geometry and lighting simple.

## Renderer Expectations

The fixed-function path must:

- preserve each cube material's authored `base-color`
- apply the current directional-light plus ambient response consistently
- remain stable across repeated per-draw GU state changes
- remain opaque for every cube
- avoid state leakage between draws

The scene should not require any schema change, cooked-asset divergence, or scene-specific runtime hack.

## Primary Risks

The most likely failure classes are:

1. per-draw material color state leaking across cubes
2. GU lighting state not remaining stable across repeated draws
3. some cubes rendering white because the wrong color state is bound
4. transform or normal handling diverging across rotated instances
5. stale startup-scene packaging causing validation against the wrong scene

## Verification

Verification is scene-driven:

1. package `colored_cube_grid` as the active startup scene
2. rebuild PSP `EBOOT.PBP` against a fresh normalized generated-core root
3. install the full cooked scene payload into PPSSPP
4. launch PPSSPP
5. stop and ask the user what they see before drawing conclusions
6. use boot logs and captured artifacts only as supporting evidence

Success means:

- PSP reaches `RuntimeMainLoop`
- all cubes are visible
- cube colors match authored intent
- directional lighting is stable across the grid
- rotation still works
- no transparency or white-material regression appears

## Implementation Strategy

Start by treating this as a verification milestone on top of the current fixed-function renderer. If the scene already works, avoid unnecessary renderer churn. If it fails, fix only the actual multi-draw fixed-function defect exposed by `colored_cube_grid`, most likely in material color binding or GU state lifetime.
