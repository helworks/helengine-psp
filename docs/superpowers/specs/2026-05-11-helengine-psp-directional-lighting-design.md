# Helengine PSP Directional Lighting Design

## Summary

Add the first real scene-driven lighting path to the PSP renderer by reading directional lights from the live runtime scene and applying ambient plus diffuse Lambert shading to lit materials. The first implementation should use CPU-side vertex lighting, but the renderer and cooked asset contract must be structured so the same scene and material data can later drive a PSP fixed-function lighting path without changing authored content semantics.

## Goal

Create a PSP lighting foundation that:

- reads real scene lights from runtime components
- supports the directional-light test scene as the first lighting milestone
- renders lit geometry with ambient plus diffuse response instead of flat white output
- keeps unlit materials working
- preserves a renderer architecture that can switch between CPU lighting and PSP fixed-function lighting
- keeps cooked asset data rich enough for either execution path
- establishes a scalable material and lighting contract that can later expand to point lights, spot lights, specular, emissive, and shadow settings

## Non-Goals

- implement point-light or spot-light contribution in this milestone
- add specular highlights or emissive contribution
- add shadow rendering
- introduce authored scene ambient settings yet
- split PSP cooked assets into CPU-only and fixed-function-only variants without profiling evidence
- change the startup-scene boot and asset-load path that is already working

## Constraints

- Scene lighting must come from the existing runtime scene/component model instead of PSP-only scene hacks.
- Ambient lighting does not currently exist as an authored scene contract, so the first milestone must use a renderer-owned default.
- The PSP renderer must not discard mesh normals or material lighting intent if that data is needed by future lighting backends.
- CPU lighting is the first implementation path, but backend selection must remain a renderer decision rather than a cooked-asset contract change.
- Failures must stay explicit. Missing required lighting inputs for a lit path should fail or fall back through an intentional contract, not through silent data fabrication.

## Existing State

The current PSP renderer successfully loads the startup scene, reaches the runtime main loop, and draws the rotating cube. The remaining visual problem is that the mesh renders as flat white because the current PSP path:

- stores only positions and indices in the runtime mesh record
- stores only base color in the runtime material record
- ignores all light components in the scene
- submits a single GU color per drawable

The generated runtime data already contains the inputs needed for the first lighting milestone:

- `ModelAsset` already carries `Positions`, `Normals`, `Indices16`, `Indices32`, and `TexCoords`
- generated core already includes `DirectionalLightComponent`, `PointLightComponent`, and `SpotLightComponent`
- light components already expose `Color`, `Intensity`, `Range`, cone angles where applicable, and parent transform

The gap is therefore in the PSP builder/runtime material contract and the PSP renderer implementation, not in scene serialization availability.

## Chosen Approach

Implement directional lighting as a scene-driven, backend-agnostic PSP lighting system with:

- a renderer-owned lighting settings object
- a per-frame scene-light snapshot
- a runtime material contract that describes lighting response independently from the execution backend
- a CPU vertex-lighting implementation for the first milestone
- a planned fixed-function backend that consumes the same scene/material contract later

The CPU lighting path should compute per-vertex ambient plus Lambert diffuse modulation from the authored base color and mesh normals. The fixed-function path should remain a later renderer backend option rather than a new asset contract.

## Architecture

### PSP Lighting Settings

Add a PSP renderer lighting-settings layer that is owned by the PSP renderer and not by scene content.

The first settings contract should include:

- `AmbientIntensity`
- `LightingPipeline`

Initial behavior:

- `AmbientIntensity` defaults to `0.25`
- `LightingPipeline` defaults to `CpuVertexLambert`

This settings object becomes the place where PSP-specific scalability decisions live. Future settings may choose cheaper or richer lighting pipelines without changing scene assets.

### Scene Lighting Snapshot

Add a PSP scene-light snapshot that is built from live runtime scene state once per camera or once per rendered frame.

The first milestone snapshot should resolve:

- the first active directional light in the scene
- light direction from the light entity orientation
- directional light color
- directional light intensity

Point lights and spot lights should not contribute yet, but the snapshot structure should be named and organized broadly enough that those light types can be added later without redesigning the renderer flow.

### Runtime Material Contract

The PSP runtime material path must stop treating materials as only a packed GU color.

For the first lighting milestone, PSP cooked and runtime material data should carry:

- base color
- receives-lighting flag
- lighting-response mode
- unlit override when needed

The runtime renderer should interpret this data through a PSP runtime material record that remains renderer-agnostic. CPU and fixed-function paths should both derive their backend behavior from the same runtime material semantics.

The first lighting-response values should be:

- `Unlit`
- `LitDirectional`

This gives the renderer an intentional branch for lighting behavior and avoids encoding lighting assumptions into ad-hoc GU state.

### Runtime Mesh Contract

The PSP runtime mesh record must preserve at least:

- positions
- normals
- indices
- texcoords when present

The first directional-light milestone only requires positions, normals, and indices to shade the cube correctly, but preserving texcoords keeps the cooked/runtime contract aligned with future material growth.

### Lighting Backend Abstraction

Separate lighting evaluation from PSP draw submission.

The renderer should be organized so that:

1. scene lights are resolved into a PSP lighting snapshot
2. drawables resolve their world transform, runtime mesh, and runtime material
3. a lighting evaluator computes shaded vertex data or backend-specific lighting state
4. a submission stage sends the result to GU

The first backend is:

- `CpuVertexLambert`

The planned later backend is:

- `FixedFunctionLambert`

The important contract is that both backends consume the same scene-light snapshot and runtime material semantics. This allows A/B switching between CPU and fixed-function lighting without changing authored content or the cooked asset layout.

## Lighting Model

### Ambient

Ambient lighting is not currently authored in the scene contract, so the first milestone must use a renderer-owned default.

Default:

- `AmbientIntensity = 0.25`

This should be stored in PSP lighting settings rather than hardcoded inside the Lambert function so that it can later be overridden by renderer settings or replaced by an authored environment/scene ambient contract.

### Directional Diffuse

The first lighting equation should be:

`finalColor = baseColor * (ambient + max(0, N dot L) * lightColor * lightIntensity)`

Where:

- `N` is the transformed and normalized surface normal
- `L` is the normalized directional-light vector
- `ambient` is the renderer default ambient intensity
- `lightColor` is the authored directional-light color
- `lightIntensity` is the authored directional-light intensity

The result should be clamped before converting to PSP vertex color output.

### Unlit Materials

Materials marked unlit should bypass scene lighting and render from their authored base color only.

This preserves explicit material intent and avoids forcing every material through the lit path.

## Cooked Asset Strategy

PSP cooked assets should preserve the richest common lighting contract that both CPU and fixed-function backends can consume.

For this milestone that means:

- keep normals in mesh data
- keep base color in material data
- add material lighting flags instead of collapsing to a single flat-color output

The build pipeline should not yet fork assets into CPU-specific and fixed-function-specific variants. Backend specialization should happen at runtime first.

Future cook-time specialization is allowed only when profiling proves PSP needs it. If that happens, specialization should be expressed as an explicit PSP profile or quality variant, not as the default contract.

This keeps the asset contract aligned with the higher-end design goal while still allowing PSP-specific downscaling later.

## Directional-Light Milestone

The first PSP lighting milestone should deliver:

- directional light read from the live scene
- ambient default from PSP renderer settings
- per-vertex CPU Lambert shading
- lit materials rendered from base color modulated by ambient plus diffuse
- unlit materials rendered without lighting
- continued support for the rotating cube startup scene path

It should not attempt:

- multiple directional-light blending
- point lights
- spot lights
- specular
- emissive
- shadows
- authored ambient/environment settings

## Testing And Verification

### Builder And Data Tests

Add builder coverage for the PSP material cook path so the new lighting fields are preserved correctly.

Add runtime or unit coverage where practical for:

- material lighting-response decoding
- receives-lighting flag behavior
- mesh/runtime data retaining normals

### Renderer Logic Tests

Add focused tests where practical for:

- directional-light snapshot resolution selecting the expected live light
- default ambient fallback equal to `0.25`
- Lambert evaluation for front-facing, grazing, and back-facing normals
- unlit-material bypass behavior

### Runtime Verification

Verify in PPSSPP using the existing directional-light test scene that:

1. the scene still boots and reaches the runtime main loop
2. the rotating cube is no longer flat white
3. lit faces respond to directional-light orientation
4. changing light direction changes the visible lit side correctly
5. unlit materials still render predictably

Later, when fixed-function support is added, verify that the same cooked assets and scene content render through both lighting pipelines.

## Risks

- PSP fixed-function constraints may eventually require different batching or vertex packing than the CPU path.
- Material schema expansion must be done carefully so the first lighting fields remain compatible with later richer material behavior.
- If mesh normals are missing or malformed in some authored content, lit rendering may expose asset-quality issues that unlit rendering previously hid.
- Additional light types may pressure the renderer to introduce culling or light-selection policies sooner than planned.

## Recommendation

Implement the directional-light milestone as a clean PSP lighting foundation with:

- renderer-owned ambient default
- scene-driven directional-light snapshot
- runtime material lighting semantics
- normals preserved end to end
- CPU vertex Lambert shading first
- a renderer pipeline switch that keeps the door open for a later PSP fixed-function lighting backend

This is the smallest real design that solves the current flat-white rendering problem while preserving a credible long-term renderer contract.
