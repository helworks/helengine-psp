# PSP Physics Scene-Binding Trace Design

## Goal

Identify the exact physics entity whose runtime binding terminates the PSP player on Adrenaline while loading `test_scene_dynamic_stack_boxes`.

## Evidence

The real-device boot trace reaches construction of `PhysicsDemoGround`, the shared cube model, and the four box materials, then terminates before `SceneState` reports `test_scene_dynamic_stack_boxes`. The scene therefore fails during scene-load activation, before its first draw or simulation step.

## Options Considered

1. Add host-only begin/end logging around `Core::Update`.
   This confirms the existing coarse boundary but cannot identify a physics body.

2. Add tracing directly to generated C++.
   This would expose the desired binding sequence but violates the repository rule against editing generated output.

3. Emit maintained runtime binding callbacks from `helengine.bepu`, then have the PSP host forward those callbacks to `PspBootTrace`.
   This keeps diagnostics at the owned source layer, identifies each entity and binding phase, and avoids changing runtime behavior. This is the selected approach.

## Design

The BEPU runtime registration source will expose a temporary, optional diagnostic callback that reports these ordered boundaries:

- scene binding begins;
- an entity with a supported rigid-body/collider pair is about to bind;
- that entity has bound successfully; and
- scene binding completes.

The PSP host will register a callback that writes each message through `PspBootTrace`. The callback remains absent on other platforms, preserving existing behavior. Entity names will be included when available, along with the body and collider types.

The callback is observational only: it must not allocate a default world, change attachment policy, catch failures, or suppress exceptions.

## Validation

1. Add source-level tests proving that the callback receives ordered begin/entity/success/end events for a static and dynamic box scene.
2. Run those tests red before implementation and green afterward.
3. Build the PSP artifact and reproduce on Adrenaline.
4. Inspect `helengine_psp_boot.log`; the final event determines the failing bind operation for the follow-up root-cause fix.

## Non-Goals

- changing BEPU scheduling, solver settings, or physics behavior;
- changing serialized scene data;
- editing generated C++ files; or
- treating the diagnostic probe as the final fix.
