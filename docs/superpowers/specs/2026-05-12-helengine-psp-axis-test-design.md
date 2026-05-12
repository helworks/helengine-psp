# Helengine PSP Axis Test Design

## Goal

Render the real city `axis_test` scene on PSP in PPSSPP using the existing PSP 3D renderer contract:

- solid-color lit materials
- CPU directional lighting
- authored scene camera
- authored cube-mesh scene content

This milestone is a verification milestone first. It should not add new PSP renderer feature surface unless `axis_test` exposes a real compatibility gap.

## Non-Goals

This milestone does not add:

- text rendering
- a new material schema
- a new light type
- scene-specific PSP runtime hacks
- speculative renderer refactors unrelated to `axis_test`

## Current Context

The PSP renderer already supports:

- runtime startup through authored startup scenes
- scene materialization from cooked city content
- solid-color base materials
- diffuse textures
- CPU directional ambient plus diffuse lighting
- textured and non-textured mesh submission

The authored city scene name is `axis_test`. Despite the original shorthand request, this is not a text scene. It is a 3D lit composition built from cube meshes, solid-color materials, a directional light, and an authored camera.

## Recommended Approach

Use the current PSP renderer as-is and treat `axis_test` as a real authored-scene verification target.

This is the right approach because:

- `axis_test` is already within the current PSP renderer contract
- any failure would indicate a compatibility bug, not missing planned renderer scope
- it avoids hiding real integration issues behind scene-specific logic

The implementation should only change code if the scene reveals an actual incompatibility in cooking, startup scene selection, runtime materialization, or rendering.

## Runtime Contract

The PSP runtime contract for this milestone remains unchanged:

- mesh geometry is loaded from cooked model assets
- solid-color materials are loaded through the current PSP material path
- directional lighting is resolved from live scene components
- camera framing comes from the authored scene camera

No `axis_test`-specific renderer branch should be introduced.

## Verification Flow

The verification path should be:

1. point the city PSP startup-scene configuration at `axis_test`
2. export a fresh city PSP cooked payload
3. identify the generated-core root used by that export
4. normalize that generated-core root using the PSP generated-core compatibility normalizer
5. rebuild `EBOOT.PBP` against that fresh normalized generated-core root
6. sync the executable and cooked payload into the PPSSPP memstick tree
7. launch PPSSPP and capture a fresh screenshot and boot log

This keeps the milestone honest by validating the actual authored scene on a fresh payload and a clean generated-core root.

## Success Criteria

The milestone is complete when all of the following are true:

- boot log reaches `Stage complete RuntimeMainLoop`
- the authored `axis_test` scene is visibly loaded
- floor and ground geometry render
- X axis renders red
- Y axis renders green
- Z axis renders blue
- origin and axis markers render white
- directional lighting is visible across the scene
- the scene is not a black screen and not flat unlit color

## Failure Boundaries

If the scene fails, work should stop at the first concrete boundary:

- startup scene selection/export mismatch
- startup-scene asset load failure
- scene materialization failure
- material cook/runtime compatibility failure
- renderer behavior mismatch between textured and non-textured lit materials

The fix should target the underlying cause only. No scene-specific fallback behavior should be added.

## Testing Strategy

The first pass should use runtime verification only:

- fresh city export
- fresh PSP rebuild
- fresh PPSSPP screenshot
- fresh PSP boot log

If the scene fails, add the smallest regression necessary to lock the discovered compatibility contract. That regression should live in the PSP builder/runtime coverage only if it protects the real root cause.

## Commit Strategy

If `axis_test` works without code changes:

- do not create a fake implementation commit
- only commit a legitimate repeatability change if one is intentionally introduced

If `axis_test` fails and requires a fix:

- commit one focused PSP `axis_test` compatibility fix on `main`
- include the regression with the fix

Do not commit:

- screenshots
- PPSSPP logs
- temporary diagnostics
- local-only scene-selection changes unless they are intentionally part of the repository workflow

## Outcome

Best case:

- `axis_test` already works on the current PSP renderer and this milestone is verified with no engine changes

Fallback case:

- `axis_test` reveals one concrete compatibility gap, and the work fixes that gap without expanding PSP scope beyond what the scene genuinely requires
