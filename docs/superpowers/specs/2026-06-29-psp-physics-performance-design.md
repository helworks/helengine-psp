# Helengine PSP Physics Performance Design

## Goal

Reduce PSP physics scene CPU cost enough that authored physics demos feel materially more responsive on PPSSPP, even if that requires visibly lower simulation precision than desktop-class platforms.

The first tuning target is:

- fixed physics cadence: `30 Hz`
- BEPU solve velocity iterations: `2`
- BEPU solve substeps: `1`
- maximum physics catch-up steps per update: `2`

## Non-Goals

- changing shared BEPU defaults for Windows, PS2, GameCube, editor tests, or other native platforms
- redesigning authored city physics scenes
- adding per-scene physics quality presets
- introducing a new PSP-specific physics runtime implementation
- tuning renderer performance in the same change

## Current Context

PSP physics scenes were recently made functional by registering `Physics3DRuntimeComponentRegistration` in the PSP native host after core initialization. That restored simulation, but the runtime still inherits the generated-core default physics scheduler and the BEPU default solve schedule:

- `PhysicsFixedStepSeconds = 1.0 / 60.0`
- `PhysicsMaxStepsPerUpdate = 8`
- `BepuPhysicsWorld3D` default solve velocity iterations = `4`
- `BepuPhysicsWorld3D` default solve substeps = `1`

Those defaults are too expensive for the current PSP runtime budget. The result is that physics scenes simulate, but at a cost that feels far too close to Nintendo DS performance.

## Approaches Considered

### 1. Lower solver work only

Keep the `60 Hz` fixed step and reduce only BEPU solve iterations.

Pros:

- smallest behavior change
- keeps motion cadence close to current desktop behavior

Cons:

- may not reduce enough CPU cost if step frequency is the dominant problem
- still allows heavy catch-up bursts

### 2. Lower cadence only

Move PSP physics to `30 Hz` but keep the default solve schedule.

Pros:

- large CPU win from halving base step frequency
- simple host-level change

Cons:

- still spends too much work per step for PSP if large stacks or contact-heavy scenes dominate

### 3. Lower cadence and solver precision together

Move PSP physics to `30 Hz`, reduce catch-up allowance, and construct a cheaper BEPU runtime world with a reduced solve schedule.

Pros:

- strongest CPU reduction
- keeps all tuning PSP-local
- matches the explicit requirement to trade precision for responsiveness

Cons:

- stack stability and contact resolution will be visibly softer than desktop
- some authored scenes may feel less accurate

## Recommended Approach

Use approach `3`.

PSP should explicitly own a low-cost physics preset in `PspBootHost` instead of inheriting desktop-oriented defaults. This keeps the compromise local to the PSP native host while preserving current behavior everywhere else.

## Architecture

### PSP Host Owns Physics Tuning

`PspBootHost::InitializeCore` should remain the only place that applies PSP runtime performance policy. The host already constructs the runtime core and registers the 3D physics runtime. It is the right boundary for PSP-only tuning because it can:

- set `CoreInitializationOptions` before core startup
- choose how the PSP physics runtime world is created
- keep shared generated-core defaults untouched

### Scheduler Tuning

Before core initialization, `PspBootHost` should override the generated-core defaults with:

- `PhysicsFixedStepSeconds = 1.0 / 30.0`
- `PhysicsMaxStepsPerUpdate = 2`

This reduces both baseline physics frequency and worst-case catch-up bursts on slow frames.

### Runtime World Tuning

PSP should stop using the generic `Physics3DRuntimeComponentRegistration::Register(EngineCore)` path for this optimization pass, because that path always creates the default BEPU world.

Instead, PSP should:

- include the lower-level BEPU runtime registration surface
- create one world with `BepuPhysicsWorld3D::CreateWithSolveSchedule(2, 1)`
- attach it to the core
- register scene binding so newly loaded scenes still bind into the active world

This preserves the same runtime architecture while replacing only the expensive default world construction policy.

### Precision Tradeoff

The expected PSP tradeoff is explicit:

- slower simulation cadence than desktop
- lower solver quality than desktop
- better overall host responsiveness on PSP

This is acceptable because the requirement is to prioritize playability over physical fidelity on PSP.

## Validation

Validation should stay narrow:

1. add one PSP packaged-runtime source test that asserts the host includes the explicit PSP physics tuning path
2. run the targeted PSP builder test and verify it fails before the change
3. implement the host tuning and verify the test passes
4. rebuild the PSP artifact
5. launch in PPSSPP and verify the physics scenes still simulate

## Expected Result

PSP physics scenes should continue to simulate after loading, but the runtime should spend materially less CPU on BEPU stepping. The authored stacked-box and related city physics demos should feel notably more responsive on PSP, with reduced precision accepted as the platform-specific compromise.
