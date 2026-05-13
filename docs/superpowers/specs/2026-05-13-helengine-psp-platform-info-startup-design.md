# Helengine PSP Textured Startup And PlatformInfo Design

## Goal

Update the PSP runtime path so the platform boots directly into the authored `textured_cube_grid` startup scene and satisfies the newer engine `PlatformInfo` initialization contract using a PSP platform implementation that mirrors the Windows integration seam.

The PSP `PlatformInfo` implementation for this milestone is intentionally minimal:

- platform id: `psp`
- platform version: `1.0.0`

## Non-Goals

- changing the authored `textured_cube_grid` scene contents
- adding a PSP-specific menu or intermediate splash scene
- introducing PSP-only startup fallbacks that bypass the new engine contract
- expanding platform metadata beyond what the engine currently requires for boot
- refactoring unrelated PSP renderer behavior

## Current Context

The PSP player already owns a real startup path in `PspBootHost`: resolve app root, initialize generated core, load the packaged startup scene, materialize it, and enter the runtime main loop. Recent PSP work already proved the renderer path for textured cubes, fixed-function rendering, and startup-scene bring-up.

The remaining issue is contract drift. The engine now expects platform metadata through a `PlatformInfo` seam that already works end to end on the Windows path. PSP should not solve this with a one-off shortcut. It should provide the same kind of dependency through the same initialization boundary, while keeping the PSP implementation itself trivial for now.

## Recommended Approach

Add a dedicated PSP `PlatformInfo` class in the PSP platform layer and inject it through the same core initialization surface used by Windows. At the same time, ensure the PSP build/export path continues to target the authored `textured_cube_grid` scene as the generated startup scene.

This keeps the boot path honest:

- startup content is still the real authored scene
- engine initialization receives the required platform dependency
- PSP stays aligned with the cross-platform architecture instead of adding a temporary platform exception

## Architecture

### Startup Scene Selection

The PSP build/export configuration should point the generated startup manifest directly at `textured_cube_grid`. The runtime should continue to load whatever startup scene is authored into the generated manifest rather than hardcoding a scene path in `PspBootHost`.

This means the PSP player stays data-driven:

- build/export chooses the startup scene
- generated startup metadata records the scene path
- `PspBootHost` loads the packaged startup scene from that metadata

### PlatformInfo Ownership

`PspBootHost` should remain an orchestration class. It should not become the home of platform metadata logic.

Add a PSP platform-info type under the PSP platform layer that owns the metadata required by core initialization. Its responsibilities are:

- report platform id `psp`
- report platform version `1.0.0`
- satisfy the engine-facing `PlatformInfo` contract through the same seam used on Windows

### Core Initialization Flow

`PspBootHost::InitializeCore` should construct the PSP render managers, input backend, and PSP `PlatformInfo`, then pass all required platform dependencies into the generated core initialization path.

If the engine initialization contract now requires `PlatformInfo`, PSP must provide it explicitly. The runtime must not:

- synthesize hidden defaults deeper in startup
- silently omit platform info
- create alternate PSP-only initialization behavior that diverges from Windows

### Failure Behavior

If PSP cannot satisfy the required initialization contract, startup should fail immediately and surface the failure through the existing PSP fatal diagnostic path.

Allowed explicit values for this milestone:

- `psp`
- `1.0.0`

Disallowed behavior:

- fallback platform ids
- empty or invented replacement values when the platform-info object is required
- "best effort" continuation after failed platform initialization

## Code Shape

Keep the existing boundaries intact:

- `PspBootHost`: startup orchestration only
- PSP `PlatformInfo` class: platform metadata only
- PSP render managers: rendering only
- PSP input backend: input only

Follow repository rules:

- one class per file
- XML comments on classes and members
- no local helper functions
- no hidden default-object creation when a valid dependency is required

## Testing And Verification

Verification should prove both parts of the change:

1. the startup-scene pipeline still targets `textured_cube_grid`
2. PSP now satisfies the engine `PlatformInfo` dependency through the correct seam

Expected checks:

- focused automated coverage for any PSP builder/export configuration or initialization code that now carries `PlatformInfo`
- direct inspection of generated startup metadata to confirm it points at `textured_cube_grid`
- rebuild of the PSP artifact
- runtime verification that PSP still reaches startup-scene load and the main loop

## Acceptance Criteria

This milestone is complete when all of the following are true:

1. the PSP export/build path produces startup metadata for the authored `textured_cube_grid` scene
2. PSP core initialization provides a real PSP `PlatformInfo` implementation instead of bypassing the dependency
3. the PSP `PlatformInfo` values are `psp` and `1.0.0`
4. PSP runtime startup still reaches startup-scene materialization and the main loop
5. no PSP-only fallback path is added to hide missing `PlatformInfo`

## Risks

- The engine-side `PlatformInfo` contract may have changed in more than one place, so PSP may need a small signature update across boot-host-owned initialization code.
- The current working tree already contains PSP edits, so implementation must be careful to integrate with in-progress changes rather than overwrite them.
- If the build/export configuration for PSP no longer targets `textured_cube_grid`, fixing startup behavior may require touching builder-side configuration in addition to runtime code.

## Recommendation

Implement this as a narrow parity change:

1. keep PSP startup data-driven and pointed at `textured_cube_grid`
2. add the minimal PSP `PlatformInfo` implementation
3. inject it through the same seam Windows uses
4. verify startup still reaches the real runtime main loop

This is the smallest correct change that restores engine contract alignment without diluting the PSP platform boundary.
