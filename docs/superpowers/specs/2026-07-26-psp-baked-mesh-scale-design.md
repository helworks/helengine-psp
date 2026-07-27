# PSP baked mesh scale design

## Goal

Remove the PSP renderer's per-frame CPU vertex scaling for static, non-uniformly scaled scene meshes while preserving fixed-function directional lighting and authored physics transforms.

## Authoring contract

The PSP platform exposes a `Bake Scale` setting beside the existing per-platform MeshComponent tessellation settings.

- It is disabled by default.
- It applies only to render geometry for the PSP cooked output.
- It is intended for static scene meshes. A bake-scaled mesh must not change its entity scale at runtime.
- Physics components keep the authored entity transform and therefore retain their existing collider behavior.

## Cook pipeline

For each PSP MeshComponent with `Bake Scale` enabled, the scene cooker creates a render-model variant specific to the entity's authored scale.

1. Transform vertex positions by the authored scale.
2. Transform normals by the inverse scale and normalize them.
3. When tessellation is enabled, tessellate this scaled geometry. Tessellation therefore observes final PSP-space edge lengths.
4. Record on the cooked render binding that the model already contains the entity scale.

Meshes with the same source model but different bake scales receive separate cooked variants. Equal source-model and scale pairs reuse one variant.

## Runtime behavior

The PSP renderer uses rotation and translation, but omits scale, for a baked-scale render binding. It submits the immutable cooked vertex stream directly to the GU. It does not allocate, copy, or CPU-scale vertices per frame.

Normal render bindings retain the current transform path. Non-uniform dynamic scaling remains supported by the existing runtime fallback unless a mesh is explicitly marked `Bake Scale`.

## Validation

- Reject a bake-scaled binding whose runtime entity scale differs from the cooked scale.
- Verify baked positions and corrected normals in the PSP cook tests.
- Verify tessellation runs after baking when both options are enabled.
- Verify cooked physics binding data retains the authored transform scale.
- Verify the PSP renderer selects the no-scale model matrix for baked render bindings.
- Re-profile Tilt Play 01: the 3D visit/queue time should no longer be dominated by per-frame scaled vertex creation.

## Scope

This is PSP-only platform cook metadata and rendering behavior. It does not change the shared entity transform schema, physics data, or other platforms.
