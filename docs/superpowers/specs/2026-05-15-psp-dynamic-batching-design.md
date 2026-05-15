# PSP Dynamic Batching Design

## Goal

Reduce CPU-side 3D submission cost on PSP for scenes with many compatible dynamic drawables, such as `colored_cube_grid`, without introducing scene-specific behavior. The solution must be engine-side and exposed through the existing platform graphics settings pipeline.

## Problem

The current PSP 3D renderer already prebuilds PSP-ready model streams, which removed the worst per-frame mesh repacking cost. The remaining overhead in simple scenes comes from repeated per-draw submission work:

- one world transform setup per drawable
- one fixed-function draw submission per drawable
- repeated render-state work across compatible drawables

For `colored_cube_grid`, this leaves roughly `16` dynamic cube draws that all use the same model and render path while only changing transform and color. That is still much slower than it should be for PSP-class geometry.

## Requirements

- The optimization must be engine-side, not scene-specific.
- The behavior must be controlled by a PSP graphics setting on `psp-forward`.
- The setting must default to `true`.
- Dynamic drawables must remain fully supported.
- Per-instance transforms and per-instance colors must continue to work.
- Incompatible drawables must safely fall back to the existing per-draw path.
- Visual output must remain unchanged.

## Recommended Approach

Add PSP dynamic batching for compatible fixed-function 3D drawables, controlled by a `psp-forward` graphics option such as `dynamic-batching-enabled`.

When enabled, the PSP renderer will group compatible dynamic drawables into per-frame transient batches, transform their vertices into one combined PSP-ready stream, and submit one draw call per batch instead of one draw call per object.

This keeps the optimization engine-side, lets builds opt out through the normal graphics settings model, and targets the actual remaining hotspot: repeated draw submission and state churn.

## Alternatives Considered

### 1. Always-On Dynamic Batching

This removes the need for a setting, but it also removes a clean escape hatch for debugging and regression isolation. Because this changes renderer behavior materially, the PSP profile should retain a switch.

### 2. Scene-Specific Batching

This is explicitly out of scope. The renderer must optimize compatible drawables generically, not through special handling for `colored_cube_grid`.

### 3. Static Batching

Static batching is useful for non-moving geometry, but it does not fit this scene because the cubes rotate every frame. The next optimization has to support dynamic transforms.

## Setting Model

The new option will be declared on the PSP graphics profile in `PspPlatformDefinitionFactory` alongside the existing `default-width`, `default-height`, `vsync-enabled`, and `fullscreen-enabled` options.

Proposed setting:

- id: `dynamic-batching-enabled`
- display name: `Dynamic Batching Enabled`
- kind: `Boolean`
- default: `true`

The existing editor selection and build-request pipeline will carry this setting through `selectedGraphicsOptionValues` without adding a new configuration path.

## Renderer Design

### Compatibility Key

Dynamic batching will only combine drawables when they share the same effective PSP render pipeline state. The batch key should include:

- runtime model identity or compatible vertex layout identity
- render path identity
- texture binding identity
- lighting mode identity
- material pipeline state that affects GPU setup

The key must not include per-instance values such as transform or color.

### Per-Instance Differences

The batching path must still allow:

- unique world transforms per drawable
- unique material color or vertex color per drawable

This is the core reason to treat the system as dynamic batching rather than static batching.

### Batch Build Strategy

For each frame:

1. Collect compatible fixed-function drawables during PSP 3D submission.
2. Partition them by compatibility key.
3. For each batch:
   - transform the source vertices for each instance into one transient combined stream
   - write per-instance color into the combined stream when needed
   - append the transformed vertices into one PSP-ready batch buffer
4. Submit one draw for the whole batch.

The current fallback path remains available for:

- disabled batching
- unsupported material/state combinations
- drawables that cannot safely share a batch

### Scope Boundaries

The first implementation should target the existing fixed-function PSP path only. It should not attempt to redesign higher-level scene traversal, material authoring, or platform-independent renderer abstractions in the same pass.

## Data Ownership

The persistent model data will remain on `PspRuntimeModel`, which already holds PSP-ready render data. Dynamic batching adds only transient per-frame batch buffers in the PSP renderer. It must not move the renderer back toward storing raw mesh data outside the runtime model.

## Error Handling

- Missing or malformed graphics setting values must resolve to the declared default.
- Unsupported drawables must skip batching and use the current path.
- Batch creation must preserve existing failure behavior for invalid renderer state; the optimization must not silently hide real errors.

## Verification

### Functional

- `colored_cube_grid` still renders correctly with rotating cubes and distinct colors.
- Disabling the graphics option restores the current per-draw path.
- Enabling the option does not change visual output in other PSP scenes that use compatible drawables.

### Performance

The PSP profiler should show:

- fewer 3D draw submissions
- lower 3D camera total time
- unchanged visual output

For `colored_cube_grid`, the expected direction is that the `16` separate cube draws collapse into a much smaller number of dynamic batches, ideally one batch for the cubes.

## Testing Strategy

- Add builder/platform-definition coverage for the new graphics setting on `psp-forward`.
- Add PSP renderer-side coverage where practical for batch compatibility selection and fallback behavior.
- Reprofile `colored_cube_grid` in PPSSPP with the setting enabled and disabled.

## Non-Goals

- Scene-specific optimization logic
- Static batching for authored static geometry
- New material authoring concepts
- Cross-platform batching architecture in this pass

## Success Criteria

This design is successful when:

- PSP dynamic batching is controlled through a standard graphics setting
- compatible dynamic drawables batch automatically in the PSP renderer
- `colored_cube_grid` shows a meaningful drop in pure 3D frame time
- disabling the setting cleanly restores the previous behavior
