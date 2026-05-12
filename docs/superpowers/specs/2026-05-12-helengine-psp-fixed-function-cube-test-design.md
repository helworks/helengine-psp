# Helengine PSP Fixed-Function Cube Test Design

## Goal

Make the PSP `cube_test` scene render through the PSP GU fixed-function lighting pipeline instead of the current CPU vertex-lighting path.

This milestone should validate:

- one cube
- one directional light
- one authored camera
- one base-color lit material
- stable rotating-scene output

## Non-Goals

This milestone does not add:

- textured fixed-function rendering
- multi-scene fixed-function compatibility
- point lights
- spot lights
- specular
- emissive
- shadows
- runtime UI for switching pipelines
- authored schema changes tied to the backend

## Current Context

The PSP renderer already supports:

- runtime startup through real authored scenes
- CPU directional Lambert lighting
- base-color materials
- diffuse textures
- scene traversal and mesh submission

The next renderer milestone is not a new material schema. It is a new execution backend for the existing lit-material contract.

The CPU path remains in the codebase as an alternative and reference, but it is not the focus of this milestone.

## Recommended Approach

Force the PSP renderer to use a `FixedFunctionLambert` pipeline for this milestone and validate only `cube_test`.

This is the right approach because:

- it keeps authored material semantics unchanged
- it keeps the backend decision inside the PSP renderer instead of in scene data
- it gives a clean, low-noise bring-up target
- it avoids mixing fixed-function bring-up with textured material complexity

The milestone should not attempt broader scene support until the fixed-function cube path is stable.

## Material And Runtime Contract

The authored contract remains semantic:

- base color
- receives lighting
- directional-lit response

The fixed-function backend consumes those semantics and translates them into PSP GU light/material state.

This means:

- no `cpu-lit` or `gpu-lit` schema names
- no backend-specific authored material ids
- no recooking materials just to switch backend

## Renderer Design

The PSP renderer should keep its lighting-pipeline layer and force the active mode to fixed-function for this milestone.

Expected pipeline structure:

- `CpuVertexLambert` remains implemented but inactive
- `FixedFunctionLambert` becomes the active path for `cube_test`

The fixed-function path should:

- stop baking diffuse lighting into vertex colors for the active path
- configure GU lighting state per draw
- enable one directional light
- set material ambient and diffuse color from the authored base color
- use the current renderer ambient default
- submit positions and normals required by the GU lighting path

## Scene Scope

The only validation scene is `cube_test`.

That scene should be used to validate:

- visible rotating cube
- correct camera framing
- correct directional-light response
- stable base-color preservation
- no black-screen or whitewashed regressions

No other scene should be used to expand milestone scope until `cube_test` is stable.

## Verification Flow

The verification path should be:

1. force PSP runtime to the fixed-function pipeline
2. point the local PSP startup scene at `cube_test`
3. export a fresh city PSP payload
4. normalize the fresh generated-core root
5. rebuild `EBOOT.PBP`
6. install the executable and cooked payload into PPSSPP
7. launch PPSSPP and capture a boot log and screenshot

## Success Criteria

The milestone is complete when all of the following are true:

- PSP boots `cube_test`
- boot log reaches `Stage complete RuntimeMainLoop`
- the cube is visible
- the cube is rotating
- the cube is lit by GU fixed-function lighting rather than CPU-baked vertex lighting
- ambient fill is present
- directional lighting is stable and visually correct
- base color is preserved
- there is no black-screen regression
- there is no flat white regression

## Failure Boundaries

If the fixed-function milestone fails, stop at the first concrete boundary:

- runtime startup failure
- cube visible but unlit
- cube visible but light direction wrong
- cube visible but washed out or white
- cube missing due to matrix, normal, or GU state issues

Fixes should target the first real boundary only. Do not mix multiple renderer changes at once.

## Testing Strategy

The first pass should use runtime verification on `cube_test`.

If needed, add targeted PSP renderer or builder regressions only when a real fixed-function compatibility contract is discovered.

Verification evidence should include:

- fresh PSP rebuild output
- fresh PPSSPP boot log
- fresh PPSSPP screenshot

## Outcome

Best case:

- `cube_test` works through GU fixed-function lighting and becomes the stable baseline for broader PSP fixed-function work

Fallback case:

- `cube_test` exposes one concrete fixed-function integration bug, and that bug is fixed without expanding the scope into textured scenes or broader material work
