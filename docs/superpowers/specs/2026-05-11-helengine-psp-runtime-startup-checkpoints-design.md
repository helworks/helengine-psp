# Helengine PSP Runtime Startup Checkpoints Design

## Summary

Advance the PSP player beyond the isolated blank-frame milestone by reintroducing runtime startup in explicit checkpoints. The PSP executable must keep the current generated-core build contract intact, preserve isolated boot as the known-good default, and provide immediate on-screen failure diagnostics when runtime startup fails. The purpose of this milestone is to prove whether the PSP player can resolve its app root, initialize `Core`, load the packaged startup scene asset, and materialize that scene without falling back to another ambiguous black screen.

## Goal

Create a PSP runtime bring-up milestone that:

- keeps the isolated blank-frame boot path as the default build behavior
- adds an intentional runtime-startup mode for PPSSPP verification
- reintroduces runtime startup in ordered checkpoints with clear fault boundaries
- promotes on-screen fatal diagnostics to the primary failure signal
- preserves `PspBootTrace` as secondary evidence for post-run inspection
- treats startup-scene load completion as the green state for this milestone

## Non-Goals

- render the authored startup scene correctly
- restore the full PSP gameplay path beyond successful scene materialization
- remove generated-core compilation from the PSP executable
- make runtime-startup mode the new default before it is proven stable
- hide startup failures behind best-effort fallbacks

## Constraints

- The current generated-core native build seam in [CMakeLists.txt](/C:/dev/helworks/helengine-psp/CMakeLists.txt:1) must stay intact. The PSP executable still compiles `helengine_core_amalgamated.cpp`, the runtime manifests, and the optional code-module manifest.
- The existing isolated-boot baseline in [src/platform/psp/PspBootHost.cpp](/C:/dev/helworks/helengine-psp/src/platform/psp/PspBootHost.cpp:1) remains the known-good reference path until runtime startup is proven.
- Failures must remain explicit. Missing content, path resolution problems, and runtime-load errors should throw or halt visibly rather than silently degrading.
- This milestone is about fault isolation and startup progression, not scene rendering quality.

## Chosen Approach

Use a staged runtime startup mode layered on top of the existing isolated boot path.

The PSP player should keep two modes:

1. isolated blank-frame boot, still enabled by default
2. explicit staged runtime startup, enabled only when the build or launcher asks for it

Within staged runtime startup, `PspBootHost` should execute one checkpoint at a time and record the current stage before entering it. Each checkpoint should have a single responsibility so that any crash or exception identifies a narrow fault boundary instead of collapsing back into a generic black-screen result.

This is the smallest honest step forward. It preserves the proven isolated baseline while giving runtime startup enough structure that PSP can be compared meaningfully against the PS2 path.

## Build Contract

The build should continue to default to:

- `HELENGINE_PSP_ISOLATED_BOOT=ON`

Runtime bring-up should be opt-in through an additional setting that enables staged startup. The exact mechanism can be a compile-time definition, a CMake option, or another explicit build flag, but it must satisfy these rules:

- isolated blank-frame boot remains the default
- staged runtime startup is enabled intentionally
- the chosen build mode is easy to identify from logs or diagnostics
- the generated-core input contract does not change between modes

The recommended shape is:

- keep `HELENGINE_PSP_ISOLATED_BOOT=ON` as the default make and builder value
- add a second explicit runtime-startup switch for checkpointed bring-up builds

This avoids destabilizing every PSP build while runtime startup is still red or unknown.

## Runtime Checkpoints

The staged runtime path should be strict and linear.

### Stage 1: App Root Resolution

Responsibilities:

- resolve the PSP app root from the executable path
- set the boot-trace destination
- record the resolved app root in diagnostics

This stage must not touch `Core`, render managers, or content loading.

### Stage 2: Core Initialization

Responsibilities:

- construct `Core`
- obtain and configure `CoreInitializationOptions`
- construct the PSP render managers and input backend
- add the PSP window to the 3D render manager
- call `Core::Initialize(...)`

This stage proves that the engine-side bootstrap and PSP platform backends can come alive together.

### Stage 3: Packaged Startup Scene Asset Load

Responsibilities:

- construct the packaged asset loader with the resolved app root
- load the startup scene asset from cooked content
- verify the scene asset exists and is readable

This stage must stop at asset acquisition. It should not materialize the scene yet.

### Stage 4: Startup Scene Materialization

Responsibilities:

- pass the loaded startup scene to `SceneLoadService->Load()`
- confirm scene instantiation completed without throwing

If this stage fails after Stage 3 succeeds, the fault boundary is scene materialization and runtime load behavior rather than filesystem or packaged-asset IO.

### Stage 5: Normal Frame Loop

Responsibilities:

- enter the normal `Update()`, `Draw()`, and present loop only after Stages 1 through 4 succeed

This stage is not the success criterion for the milestone. It is simply the next legal state after startup-scene load completes.

## Failure Behavior

The primary failure signal should be immediate on-screen diagnostics.

If any checkpoint throws, returns failure, or detects a required invalid state, `PspBootHost` should halt the app and display:

- the failing checkpoint name
- the exception message or concrete failure reason
- a short note that the app is intentionally halted for diagnostics

`PspBootTrace` should remain enabled and should attempt to write the same stage progression and failure message to disk. The trace is secondary evidence, not the primary debugging mechanism, because filesystem or path issues may be part of the problem being investigated.

There should be no best-effort continuation after checkpoint failure. Startup either advances cleanly to the next boundary or halts with evidence.

## Diagnostics

At minimum, diagnostics should record:

- process entry
- graphics initialization start and completion
- selected boot mode
- stage entry for each runtime checkpoint
- stage completion for each runtime checkpoint
- fatal checkpoint failure name and reason

This should be enough to correlate PPSSPP behavior with the last confirmed startup boundary, even when the visual result is only a black or blank frame.

## Testing Strategy

Builder-side behavior should stay covered with automated tests. If the runtime-startup mode introduces a new builder, make, or Docker command contract, tests should assert that contract explicitly so the isolated default and generated-core mount behavior remain stable.

The native PSP runtime checkpoint flow is integration-oriented rather than meaningfully unit-testable in this repository. The important verification for this milestone is:

1. build the isolated blank-frame baseline successfully
2. build the staged runtime-startup variant successfully
3. launch the staged runtime-startup variant in PPSSPP
4. confirm either:
   - startup-scene load completes, or
   - the app halts on a precise checkpoint failure message

Success is not defined by rendered scene correctness yet. Success means PSP no longer fails opaquely during runtime startup.

## Verification Criteria

The milestone is green when all of the following are true:

1. the PSP executable still compiles generated-core and runtime manifest sources
2. isolated blank-frame boot remains available as the default build path
3. staged runtime startup can be enabled explicitly
4. PPSSPP reaches startup-scene load or halts with a checkpoint-specific on-screen fatal diagnostic
5. the last confirmed runtime boundary is obvious from the screen and trace output

The milestone is red if:

- enabling staged runtime startup removes or bypasses generated-core compilation
- the staged build regresses the isolated baseline
- PPSSPP still fails with only an undifferentiated black screen
- failures continue silently without stage identification

## Risks

- PSP may still fail before checkpointed runtime startup meaningfully advances, which would imply the fault is lower in graphics/bootstrap or in static initialization.
- Startup-scene load may complete but the next frame-loop step may still expose PSP-specific renderer issues. That is acceptable because it still gives a new fault boundary.
- If the staged runtime-startup switch is designed ambiguously, the team may lose track of which artifact PPSSPP is actually running. The build mode therefore must be explicit in diagnostics.

## Recommendation

Implement checkpointed runtime startup without changing the isolated default. The first implementation should favor clear stage ownership and hard-fail diagnostics over ambitious behavior restoration. Once PPSSPP proves which checkpoint fails first, compare that boundary against the PS2 platform and address the actual mismatch instead of guessing across the whole startup path.
