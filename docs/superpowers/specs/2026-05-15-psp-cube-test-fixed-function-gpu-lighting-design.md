# PSP Cube Test Fixed-Function GPU Lighting

## Goal

Replace the current CPU-side directional lighting path for the narrow `cube_test` compatibility slice with a real PSP GU fixed-function lighting path.

This pass is intentionally narrow. It is not a full PSP lighting rewrite. The purpose is to prove that the PSP renderer can light compatible meshes on the GPU, keep the current fallback path for everything else, and establish the expansion path for later scenes.

## Problem

The current PSP 3D renderer computes directional lighting on the CPU in `PspRenderManager3D.cpp`:

- normals are rotated on the CPU
- ambient plus diffuse lighting is evaluated on the CPU
- the result is packed into vertex colors
- GU then only draws pre-lit vertices

That approach works as a bring-up path, but it is the wrong long-term rendering model for PSP. The user wants the PSP renderer to move toward hardware fixed-function state, where the console-specific pipeline setup is the source of truth rather than CPU-emulated lighting.

`cube_test` is the right first target because it is the smallest lit scene:

- one simple mesh
- no imported multi-material complexity
- no spotlight requirement
- no need to solve the racer material issue

## Scope

This pass only adds one new PSP renderer capability:

- GPU fixed-function directional lighting for compatible untextured lit meshes

This pass does not include:

- textured fixed-function lighting
- spot lights
- point lights
- shadows
- specular
- submesh or material-slot fixes
- scene-specific hacks

## Supported Compatibility Slice

The new GPU-lit path should only activate when all of these are true:

- the drawable has one runtime model with PSP-ready fixed-function vertex data
- the material is untextured
- the material lighting response is directional-lit
- the material receives lighting
- the active scene-lighting configuration is compatible with one directional light

If any of those conditions are not met, the renderer must stay on the current fallback path.

## Renderer Design

### 1. Add one GPU-lit submission path

`PspRenderManager3D` should gain one dedicated submission path for GU fixed-function directional lighting.

That path should:

- submit normals and positions to GU
- configure GU ambient and directional-light state
- configure GU material state
- draw the model without baking lit colors on the CPU

The existing CPU-lit path remains intact and becomes the compatibility fallback.

### 2. Keep platform-ready model ownership in the PSP runtime model

`PspRuntimeModel` already owns PSP-ready fixed-function vertex streams. The new path should continue using platform-specific ready-to-render data there rather than rebuilding lighting-specific buffers at draw time.

For this pass, the required vertex layout is:

- normal
- position

No new raw-geometry ownership should be added to the PSP renderer.

### 3. Configure GU lighting from the existing directional-light snapshot

The new path should reuse the current directional-light scene snapshot that PSP already resolves for rendering. For the compatible case:

- ambient intensity maps to GU ambient state
- directional-light direction maps to GU light direction
- directional-light color and intensity map to GU light color contribution

This keeps scene-light discovery unchanged in this pass. The change is only how compatible drawables are lit.

### 4. Configure GU material state from shared material intent

The compatible GPU-lit path should treat PSP material state as fixed-function render intent, not as shader logic.

For this pass, the relevant material inputs are:

- base color
- receives-lighting
- lighting-response

Those values should configure GU material color state directly. No CPU-side diffuse bake should happen for compatible drawables.

### 5. Keep fallback behavior explicit

The renderer must decide per drawable:

- if compatible, use the new GPU-lit fixed-function path
- otherwise, use the current CPU-lit or unlit path

There should be no scene-name checks and no authoring-side special cases.

## Expected Runtime Behavior

For `cube_test`, the renderer should choose the new fixed-function GPU lighting path automatically because the scene is expected to match the narrow compatibility slice.

For more complex scenes, nothing should regress:

- unsupported cases continue using the current fallback path
- startup and scene load behavior stay unchanged
- material interpretation stays stable for the existing fallback path

## Testing Strategy

### Engine and builder verification

Add focused verification around renderer path selection if there is a stable seam for it. If there is no clean automated seam in the PSP native layer, keep tests minimal and verify through native export and PPSSPP capture.

### Native verification

For the implementation pass:

- export a PSP build with `cube_test` as startup
- run PPSSPP
- verify the scene renders correctly
- verify the new path is actually selected
- compare frame timing against the current CPU-lit baseline

### Regression constraints

The implementation is acceptable only if:

- `cube_test` still renders lit geometry correctly
- unsupported scenes still boot
- fallback rendering still works for non-compatible drawables

## Risks

### Visual parity risk

GU fixed-function lighting will not match the current CPU-lit path exactly. Small differences in ambient or diffuse response are acceptable for this pass as long as the result is coherent and stable.

### State-setup risk

GU lighting state can be sticky. The new path must restore or explicitly set the required state so it does not contaminate fallback or unlit draws.

### Scope creep risk

It will be tempting to solve textured lighting, spot lights, or racer materials in the same pass. This design explicitly rejects that. The first milestone is only a narrow GPU-lit `cube_test`-compatible path.

## Success Criteria

This milestone is complete when all of these are true:

- `cube_test` renders through the new PSP GPU fixed-function directional-light path
- the current CPU-lit path remains available as fallback
- PPSSPP verification shows the compatible path is used successfully
- no scene-specific hacks are introduced
- the implementation leaves a clear expansion path for textured directional lighting later

## Follow-Up Work

After this milestone proves out, the next renderer expansions should be:

1. textured fixed-function directional lighting
2. submesh and material-slot rendering for imported multi-material models
3. spot light support

That order keeps the renderer aligned with the user’s stated priorities and avoids mixing unrelated fixes into the first GPU-lighting pass.
