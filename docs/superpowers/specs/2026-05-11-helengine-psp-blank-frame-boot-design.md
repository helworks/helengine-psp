# Helengine PSP Blank-Frame Boot Stabilization Design

## Summary

Stabilize the PSP player so PPSSPP stays alive on a blank frame, while preserving the real Helengine build contract. The PSP executable must still compile and bundle the editor-generated engine/runtime C++ sources and manifests, but the boot path for this milestone must not execute generated-core initialization, packaged-asset loading, or startup-scene load.

## Goal

Create a PSP boot milestone that:

- still builds the real `helengine_psp` player against generated engine code
- still produces a normal `EBOOT.PBP`
- launches in PPSSPP without immediately crashing
- enters a stable GU/VBlank frame loop
- clears and presents a blank frame continuously

## Non-Goals

- load the startup scene
- initialize `Core`
- execute authored runtime/script code
- render real scene content
- remove generated-core files from the native build

## Constraints

- The current native build contract in [CMakeLists.txt](/C:/dev/helworks/helengine-psp/CMakeLists.txt:1) must remain intact: generated `helengine_core_amalgamated.cpp` and runtime manifest sources stay compiled into the PSP executable.
- The PSP app entry in [src/main.cpp](/C:/dev/helworks/helengine-psp/src/main.cpp:1) remains the normal production entrypoint.
- Failures should remain explicit. We are isolating execution, not hiding missing build inputs.
- This milestone is only meant to prove boot stability in PPSSPP, not engine correctness.

## Chosen Approach

Use boot-host isolation.

`PspBootHost` should keep its PSP module startup, boot tracing, GU initialization, and frame presentation loop. For this milestone it should bypass all engine-side execution after graphics initialization. That means `Run()` initializes graphics, logs that the player entered isolated blank-frame mode, and then loops on begin-frame and present-frame only.

This is the smallest change that answers the immediate question: is the crash caused by PSP graphics/bootstrap setup, or by generated-core and runtime startup work layered on top of it? Keeping generated-core bundled while not executing it preserves the native-build contract and avoids a false positive that would come from stripping the player down into a different program.

## Runtime Design

### Build-Time Contract

The PSP native player must continue to compile:

- PSP platform/player sources
- generated `helengine.core` C++ sources
- generated runtime startup/scene-catalog manifests
- optional generated code-module manifest when present

No build-file change in this milestone should remove those inputs from the executable.

### Boot-Time Contract

The isolated boot path should:

1. enter `main`
2. construct `PspBootHost`
3. initialize GU/display state
4. enter a perpetual frame loop
5. clear color and depth
6. wait for VBlank
7. swap buffers

The isolated boot path should not:

- call `InitializeEngine()`
- resolve the PSP app root for runtime content access
- construct `Core`
- touch packaged assets
- load the startup scene
- invoke PSP render managers through generated-core draw flow

### Diagnostics

Keep `PspBootTrace` active around:

- process entry
- graphics initialization start/end
- transition into isolated blank-frame mode
- fatal exception handling

This preserves enough signal for PPSSPP log correlation without adding a larger debugging framework yet.

## Verification

Success means:

1. the PSP native build still succeeds with generated-core inputs required
2. the produced `EBOOT.PBP` launches in PPSSPP
3. PPSSPP remains running instead of crashing immediately
4. the player presents a stable blank frame

Failure means:

- generated-core sources are no longer part of the executable
- PPSSPP still crashes before or during the blank-frame loop
- the player exits instead of staying alive in the frame loop

## Risks

- The crash may still be inside low-level PSP graphics setup rather than engine startup, in which case isolation will narrow the fault but not solve it.
- If some compiled generated-core static initialization is crashing before `Run()`, preserving bundling may still reproduce the crash. That is acceptable because it proves the failure boundary more honestly than removing those objects from the build.

## Recommendation

Implement the isolated blank-frame boot path first. If PPSSPP stays alive, reintroduce runtime stages one boundary at a time. If it still crashes, focus next on GU/display/bootstrap calls and any static initialization that happens before engine startup.
