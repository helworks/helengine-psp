# PSP Fixed-Function Cube Test

Status: Planned
Date: 2026-05-12
Spec: [2026-05-12-helengine-psp-fixed-function-cube-test-design.md](../specs/2026-05-12-helengine-psp-fixed-function-cube-test-design.md)

## Goal

Make `cube_test` render through the PSP GU fixed-function lighting path with the existing `lit-directional` material semantics, while leaving the CPU lighting path in the codebase as an inactive fallback.

## Scope

- Force the active PSP lighting pipeline to `FixedFunctionLambert` for this milestone.
- Keep authored material schema and runtime lighting semantics unchanged.
- Support one directional light plus renderer ambient default.
- Verify only on `cube_test`.

## Out Of Scope

- Textured fixed-function rendering
- Point lights
- Spot lights
- Specular, emissive, or shadows
- Runtime UI for switching lighting backends
- Broad-scene compatibility beyond `cube_test`

## Tasks

- [ ] Add a failing fixed-function bring-up boundary in the renderer.
  - Update the current `FixedFunctionLambert` placeholder path in `src/platform/psp/rendering/PspRenderManager3D.cpp` so the code has a clear implementation entry point instead of the current generic runtime failure.
  - Keep the CPU vertex-lighting path intact for later fallback work.

- [ ] Force the PSP renderer to use `FixedFunctionLambert` for this milestone.
  - Set the active pipeline through the PSP lighting settings/runtime path, not through authored scene data.
  - Make the forced pipeline choice explicit and easy to revert once fixed-function is stable.

- [ ] Split CPU-lit submission from fixed-function submission in the PSP 3D renderer.
  - Preserve the existing CPU-lit vertex path for reference.
  - Add a dedicated fixed-function path that submits positions and normals instead of prelit vertex colors.
  - Keep shared scene traversal, mesh resolution, and material resolution logic outside the backend-specific submission path.

- [ ] Implement GU fixed-function lighting state for one directional light.
  - Configure PSP GU lighting enable/state per draw or per camera as appropriate.
  - Feed ambient contribution from `PspLightingSettings`.
  - Feed directional light direction, color, and intensity from `PspSceneLightingSnapshot`.
  - Match the engine directional-light contract already used by the CPU path, including direction sign semantics.

- [ ] Implement GU fixed-function material state for current lit materials.
  - Use the resolved PSP runtime material base color as the fixed-function material color input.
  - Respect `receives-lighting` and `lit-directional` behavior without changing the authored schema.
  - Ensure the cube does not wash out to flat white under the current ambient default.

- [ ] Preserve the current working transform/framing path.
  - Reuse the existing camera/model matrix flow that already renders `cube_test` correctly.
  - Verify normals and matrix state are correct together under the fixed-function pipeline.
  - Fix only the real matrix/normal/state issue if the cube disappears or lights incorrectly.

- [ ] Add the smallest useful regression coverage.
  - Add or update PSP renderer-side tests only where they protect a concrete contract exposed by this work.
  - Do not invent builder coverage for behavior that only exists in runtime GU state.

- [ ] Rebuild and verify the fixed-function milestone on hardware-equivalent runtime.
  - Export a fresh city PSP payload targeting `cube_test`.
  - Normalize the generated-core root before rebuilding `EBOOT.PBP`.
  - Rebuild the PSP app with runtime startup enabled.
  - Install into `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE`.
  - Launch PPSSPP and capture:
    - boot log
    - screenshot
  - Verify:
    - `RuntimeMainLoop` is reached
    - the cube is visible and rotating
    - fixed-function lighting is active
    - directional lighting is stable
    - ambient fill is present
    - there is no black-screen or flat-white regression

## Verification Commands

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal
```

```powershell
& {
    $env:HELENGINE_PSP_REPOSITORY_ROOT = 'C:\dev\helworks\helengine-psp'
    dotnet 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll' --project 'C:\dev\helprojs\city' --build psp --output 'C:\tmp\city-psp-fixed-function-cube-test-build'
}
```

```powershell
rtk dotnet run --project builder/helengine.psp.builder.csproj -- --normalize-generated-core <generated-core-root>
```

```powershell
rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v <generated-core-root>:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

## Failure Boundaries

- Startup/runtime failure before `RuntimeMainLoop`
- Cube renders but remains unlit
- Cube renders but directional light direction is wrong
- Cube renders but base color washes out to white
- Cube disappears because fixed-function matrix, normal, or GU state is incorrect

## Exit Criteria

- `cube_test` reaches `RuntimeMainLoop`
- the rotating cube is visibly rendered through PSP fixed-function lighting
- ambient plus directional response is stable
- the existing CPU path remains available in code for future fallback work
