# PSP Textured Cube Grid Fixed-Function Design

## Goal

Make `textured_cube_grid` render through the PSP fixed-function pipeline with authored textures, directional lighting, ambient fill, and correct material color modulation.

## Scope

- Keep `FixedFunctionLambert` forced as the active PSP lighting pipeline.
- Validate only `textured_cube_grid`.
- Keep the authored PSP material contract unchanged:
  - `base-color`
  - `texture-id`
  - `receives-lighting`
  - `lit-directional`
- Keep the scene opaque and non-mipmapped for this milestone.

## Out Of Scope

- Mipmaps
- Filtering beyond current nearest sampling
- Transparency
- Specular
- Emissive
- Point lights
- Spot lights
- Runtime backend switching

## Architecture

The authored scene and cooked material contract stay semantic and unchanged. The PSP runtime remains responsible for how those semantics are executed, and this milestone extends the current fixed-function path so textured materials no longer fall back to CPU behavior or throw.

The renderer must combine three existing concepts under one fixed-function path:

1. texture sampling
2. directional plus ambient lighting
3. base-color material modulation

The target scene is the real authored `textured_cube_grid`, not a reduced debug scene. `cube_test` already proved untextured fixed-function lighting works, and earlier work already proved textured CPU rendering works on PSP. The remaining problem is their combination under GU fixed-function.

## Renderer Expectations

The fixed-function textured path must:

- bind each cube's authored texture correctly
- preserve per-material base-color modulation
- preserve directional-light plus ambient response
- submit UVs, normals, and positions in the correct fixed-function vertex layout
- keep texture state and lighting state stable across repeated draws
- avoid white, black, or incorrectly shared textured cubes

The scene should remain opaque and continue using nearest texture sampling for now.

## Primary Risks

The most likely failure classes are:

1. wrong fixed-function textured vertex layout or attribute ordering
2. GU texture function combining texels and lighting incorrectly
3. per-draw texture state leaking between cubes
4. lighting disappearing once textures are enabled
5. base-color modulation washing textures out to white or darkening them incorrectly
6. stale executable install masking the rebuilt runtime

## Verification

Verification is scene-driven and install-order-sensitive:

1. retarget the PSP export to `textured_cube_grid`
2. export a fresh PSP payload
3. normalize the fresh generated-core root
4. rebuild `build/EBOOT.PBP`
5. install cooked/material payload first
6. install rebuilt `build/EBOOT.PBP` last so it does not get overwritten by an older exported executable
7. launch PPSSPP
8. stop and ask the user what they see before drawing conclusions
9. inspect logs only as supporting evidence

Success means:

- PSP reaches `RuntimeMainLoop`
- all cubes are visible
- textures are correct and distinct
- directional lighting remains active
- base-color modulation behaves correctly
- rotation still works
- the scene stays opaque

## Implementation Strategy

Treat this as a vertical slice on top of the current fixed-function renderer. Extend the fixed-function branch so textured materials participate in GU texture sampling and GU lighting at the same time. If the first run fails, fix only the specific fixed-function texture or state-management defect exposed by `textured_cube_grid`, not the authored schema or unrelated renderer systems.
