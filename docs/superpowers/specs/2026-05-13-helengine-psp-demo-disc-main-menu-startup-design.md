# PSP Demo Disc Main Menu Startup Build Design

## Goal

Produce a fresh PSP build that boots directly into the city demo disc main menu instead of the textured cube grid startup scene.

## Scope

- Update the city PSP persisted build selection so `DemoDiscMainMenu` is the only selected startup scene.
- Reuse the existing PSP builder and runtime startup path already merged into `main`.
- Regenerate the PSP export through the editor CLI.
- Install the rebuilt `PSP/GAME/HELENGINE` output into the PPSSPP memstick tree for verification.

## Non-Goals

- Do not change menu layout, menu content, or menu scene generation logic.
- Do not add the full demo scene catalog to the PSP packaged build in this pass.
- Do not change the PSP runtime startup contract beyond the selected startup scene.

## Design

### Startup Selection

The city project stores platform scene selection in `user_settings/build_config.json`. The PSP platform entry will be updated to:

- `selectedSceneIds = [ "DemoDiscMainMenu" ]`
- `sceneOrders = [ { "sceneId": "DemoDiscMainMenu", "orderNumber": 1 } ]`

This keeps the build deterministic and makes the demo disc main menu the sole packaged startup scene for PSP.

### Build Flow

The existing headless editor CLI build flow remains the source of truth:

- use the already-configured PSP platform descriptor
- invoke the editor CLI against `C:\dev\helprojs\city\project.heproj`
- emit the PSP output tree under the configured output directory

The PSP native build path already embeds runtime startup scene metadata and platform metadata. No additional PSP code changes are required for this startup-scene swap.

### Verification

Verification should prove:

- the generated startup manifest points at the cooked `DemoDiscMainMenu` scene
- the final PSP output tree contains `EBOOT.PBP`
- PPSSPP boot log reaches the main loop
- runtime startup loads the cooked main menu scene path instead of `textured_cube_grid`

## Risks

- If the main menu scene depends on packaged scenes that are not included in this minimal build, runtime navigation may fail after startup even if boot succeeds.
- If the persisted city build config is overwritten by later editor usage, the PSP startup scene may drift again.

## Success Criteria

- A fresh PSP export is produced successfully.
- The PSP build boots into the demo disc main menu startup scene.
- PPSSPP log confirms startup-scene load and main-loop reachability without platform-info failures.
