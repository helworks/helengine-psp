## Goal

Move PSP mesh preparation out of the per-frame 3D draw path and into `PspRuntimeModel` creation so platform-specific models hold renderer-ready vertex streams instead of raw authored geometry.

## Scope

- Update `PspRuntimeModel` to own pre-expanded PSP vertex streams for the supported 3D pipelines.
- Update `PspRenderManager3D::BuildModelFromRaw(...)` to build those streams once from `ModelAsset`.
- Update the fixed-function 3D draw path to submit cached vertex buffers directly.
- Preserve current visuals and material behavior.

## Design

- `PspRuntimeModel` will store one ready-to-submit untextured fixed-function stream and one ready-to-submit textured fixed-function stream.
- `PspRenderManager3D` will stop storing per-model raw position/normal/uv/index vectors in the global `MeshRecords` cache.
- `BuildModelFromRaw(...)` will resolve the authored index stream once, expand it once, and populate `PspRuntimeModel`.
- `Visit(...)` will cast the incoming runtime model to `PspRuntimeModel` and submit the cached buffers directly for the fixed-function lighting path.
- Non-geometry per-draw state remains unchanged: world matrix, material color, texture binding, and lighting state still apply at draw time.

## Risks

- The runtime model must preserve both textured and untextured streams because PSP GU vertex layouts differ.
- Index validation needs to fail fast during model build instead of silently producing broken draw buffers.
- The current non-fixed-function CPU-lit path may still need separate follow-up work after the fixed-function path is corrected.

## Verification

- Rebuild the PSP runtime successfully.
- Re-run the colored cube grid scene in PPSSPP.
- Compare `PspPerfFrame3D` totals before and after the change.
