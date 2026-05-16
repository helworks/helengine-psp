# PSP Scaled Cube Sample Design

## Goal

Add one permanent rendering sample scene that makes PSP non-uniform-scale behavior easy to inspect.

The sample must:

- contain one static cube scaled to `float3(5f, 20f, 10f)`
- use an orbiting camera that keeps looking at the cube
- be generated through the existing city rendering-scene pipeline
- appear in the Demo Disc menu as a selectable sample
- be ordered immediately after `cube_test`, making it the second rendering sample in the list
- be included in PSP full exports with the other rendering scenes

## Scope

This change covers:

- city scene-factory code for the new sample
- rendering-scene generation registration
- Demo Disc menu inclusion and ordering
- PSP build-scene inclusion through normal project build configuration

This change does not cover:

- any PSP-specific renderer behavior changes
- new lighting models
- new materials, shaders, or textures
- scene-specific hacks for PSP

## Scene Design

The new sample is a minimal directional-light scene focused on one large non-uniformly scaled cube.

Scene contents:

- one camera entity
- one FPS overlay entity
- one directional light entity
- one cube mesh entity at the origin

Cube authored state:

- model: generated cube model
- material: generated standard material
- local position: `float3(0f, 10f, 0f)` or equivalent placement that keeps the tall cube visually centered against the ground plane choice
- local scale: `float3(5f, 20f, 10f)`
- local orientation: identity
- no rotation component

Camera authored state:

- orbit camera component
- orbit center at the cube
- stable orbit radius and height chosen so the full scaled cube is readable on PSP and desktop
- inward-facing orientation maintained by the component

Lighting authored state:

- one directional light using the same simple directional-light conventions as the other rendering samples
- no spotlight logic
- no shadows required for this sample unless existing shared sample defaults make them effectively free to keep

## Integration Design

### Scene Factory

Add one dedicated factory in the city rendering tools, following the existing pattern used by `CubeTestSceneFactory`, `ColoredCubeGridSceneFactory`, and `DirectionalShadowPlazaSceneFactory`.

Responsibilities:

- create the generated authored scene definition
- create the camera entity
- create the FPS entity
- create the directional-light entity
- create the single scaled cube entity

The scene should remain intentionally simple and avoid introducing new reusable runtime systems unless a clearly shared helper is already available.

### Scene Generator

Register the new scene in `RenderingSceneGenerator`.

Required updates:

- add a new stable scene id constant
- instantiate the new scene factory
- generate and write the new authored scene alongside the existing rendering scenes

The new sample should remain part of the permanent generated rendering set, not a hand-maintained `.helen` file.

### Demo Disc Menu

Add the scene to the rendering sample list shown in the Demo Disc menu.

Ordering requirement:

1. `cube_test`
2. new scaled-cube sample
3. existing remaining rendering samples in their current relative order

The menu addition must be data-driven through the existing menu-definition / scene-list path, not through PSP-specific code.

### Build Inclusion

Ensure the new scene is part of the normal PSP full build scene list so it is cooked and packaged with the Demo Disc content.

No special export path is needed. The sample should travel through the same city build configuration and packaging flow as the other rendering scenes.

## Data Flow

1. City rendering-scene generation creates the new authored scene.
2. The authored scene is written to the normal `scenes/rendering/...` location.
3. Demo Disc menu definitions include the new scene id in the rendering sample list.
4. PSP full export cooks and packages the scene with the rest of the Demo Disc content.
5. PPSSPP or real PSP hardware can launch the sample from the main menu like any other rendering scene.

## Error Handling

The new factory should follow current city factory behavior:

- throw if required runtime assets are missing
- avoid silent defaults for required inputs
- keep authored scene setup deterministic

No new runtime fallback behavior should be introduced.

## Testing And Verification

Minimum verification for this change:

- regenerate the city rendering scenes successfully
- confirm the new `.helen` scene file exists under `scenes/rendering`
- rebuild a full PSP export
- verify the packaged PSP output contains the new cooked scene asset
- launch PPSSPP and confirm the scene is selectable from the Demo Disc menu in the second rendering slot
- open the sample and confirm the orbit camera frames the scaled cube correctly

## Risks

### Menu Ordering Drift

The main risk is inserting the new sample without preserving the existing ordering contract for the other rendering scenes.

Mitigation:

- update only the explicit scene-list ordering path
- verify the new sample is second and the rest remain stable

### Camera Framing

The orbit camera could be authored too close or too low for the `5x20x10` scale.

Mitigation:

- choose conservative radius and height values
- validate visually in PPSSPP after export

### Generator Drift

The rendering scene generator is the source of truth for permanent rendering samples. A hand-authored file or ad hoc menu addition would drift over time.

Mitigation:

- keep the new sample fully generator-owned
- wire all references through the same generation path as the other rendering scenes

## Recommendation

Implement the sample as a normal generated rendering scene with one dedicated scene factory and one menu-list insertion immediately after `cube_test`.

That keeps the sample permanent, visible in the Demo Disc flow, and useful for PSP renderer debugging without introducing any PSP-specific content path.
