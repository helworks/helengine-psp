# Helengine PSP End-to-End City Cube Test Design

## Summary

Build the first real Helengine PSP platform path that starts from the editor build flow, uses editor-generated `helengine.core` C++ output, packages a PSP homebrew app folder ending in `EBOOT.PBP`, and boots the real `city` `cube_test.helen` scene in PPSSPP. The first milestone should render only the authored cube mesh as an unlit rotating mesh, while keeping the builder/player architecture aligned with the existing Windows and PS2 platform pattern.

## Goal

Create the first PSP platform build that:

- is selectable from the editor as a real platform target
- uses the editor-owned generated-core regeneration flow
- packages the built output as a PSP homebrew app folder
- loads the real cooked `city` `cube_test.helen` scene
- renders the cube triangles on screen in PPSSPP
- rotates the cube through the normal runtime update/script path

## Non-Goals

- build a PSP ISO or UMD-style image
- add fallback hardcoded scene rendering
- implement general PSP lighting or shadows
- support the full city scene set on day one
- implement PSP menu/UI/runtime scene switching beyond the startup scene requirement
- invent a PSP-specific build path outside the editor builder/player flow

## Constraints

- The PSP platform must follow the existing Helengine builder/player split used by the editor.
- The editor owns generated-core regeneration and writes runtime native manifests before the platform builder runs.
- PSP should use the normal cooked-relative runtime path model used by Windows unless a real PSP constraint forces a platform-specific divergence.
- The first runtime slice should be strictly limited to the features needed by the authored `cube_test.helen` scene.
- Failures should remain explicit. Missing or unsupported required runtime data must fail loudly instead of falling back to placeholders.

## Chosen Approach

Use a real PSP builder/player flow that mirrors the existing native platform architecture, but keep PSP runtime asset resolution Windows-style.

The editor will regenerate the shared generated C++ core for platform `psp`, cook the selected scene and referenced assets, write native startup and scene-catalog manifests into the generated-core tree, and then invoke a dedicated PSP builder. The PSP builder will stage a PSP homebrew app folder, compile the PSP player against the generated core, and produce `EBOOT.PBP`.

At runtime, the PSP player will resolve its app root, load cooked assets through the generated startup and scene-catalog manifests, and render the actual scene contents. The first renderer will intentionally interpret the standard material through a narrow unlit opaque path so the milestone proves the real runtime path without dragging lighting and shadow work into the first bring-up.

## Architecture

### Editor-Owned Build Graph

The editor remains the source of truth for PSP builds.

The PSP platform builder must expose a `PlatformDefinition` with:

- `platformId` set to `psp`
- at least one PSP build profile
- at least one PSP graphics profile
- a real C++ codegen profile
- storage/media metadata sufficient for a staged PSP homebrew output

The editor build graph must:

1. load the PSP builder assembly
2. read the PSP `PlatformDefinition`
3. regenerate generated core for platform `psp`
4. cook scenes and referenced assets
5. compile authored gameplay code into generated/native inputs
6. write runtime native manifest sources into the generated-core tree
7. finalize the generated-core source tree
8. invoke the PSP builder `BuildAsync(...)`

The PSP builder must consume the generated-core root supplied by the editor. It must not own or duplicate generated-core regeneration.

### Builder/Player Split

The PSP implementation is split into two responsibilities:

- `helengine.psp.builder`
  - publishes typed platform metadata to the editor
  - consumes the cooked manifest and generated-core root
  - stages the final homebrew app layout
  - invokes the native PSP build
  - verifies the built app folder
- `helengine-psp` native player
  - owns PSP boot, file access, and PSP rendering
  - compiles against the generated `helengine.core` C++ output
  - resolves runtime paths relative to the PSP app root
  - loads startup scene data and renders the live runtime scene

### Runtime Path Model

PSP should not inherit the PS2 `cdrom0:` path translation pattern.

The generated startup and scene-catalog manifests should remain cooked-relative, matching the existing Windows/runtime-native manifest pattern. Example manifest values:

- `cooked/scenes/rendering/cube_test.hasset`
- `cooked/models/...`
- `cooked/materials/...`

The PSP player resolves its app root once, then combines the app root with those cooked-relative paths during runtime asset loads.

This keeps the shared editor/runtime contract consistent across native targets and only leaves PSP-specific filesystem handling inside the PSP host.

## Repository Shape

The PSP repository should grow to include:

- `builder/`
- `builder.tests/`
- `src/`
- `docs/superpowers/specs/`
- `docs/superpowers/plans/`

The builder project should be structurally similar to the existing Windows and PS2 platform builders, but the packaging output should target a PSP homebrew app folder rather than a Windows executable folder or PS2 disc layout.

## Build And Packaging Design

### Editor Platform Registration

The main `helengine` platform installation configuration must gain a real `psp` entry that points to:

- the PSP builder assembly path
- the PSP player source root
- the generated-core C++ root
- the codegen tool path

The `city` project must also gain PSP support through its project/platform configuration so the editor can select PSP as a valid build target.

### Builder Output Layout

The first packaged PSP output should be a staged homebrew app directory shaped like:

- `output/psp/PSP/GAME/HELENGINE/EBOOT.PBP`
- `output/psp/PSP/GAME/HELENGINE/cooked/scenes/rendering/cube_test.hasset`
- `output/psp/PSP/GAME/HELENGINE/cooked/...` for the referenced cooked assets

This is the output contract to validate in PPSSPP.

### Native Build Inputs

The PSP native build should compile:

- PSP host/platform sources from this repository
- generated `helengine.core` C++ sources from the editor-provided generated-core root
- generated runtime manifest sources from the same generated-core root
- authored generated gameplay/runtime code required by the startup scene

The native build must fail clearly if the generated-core root is missing required files.

## Runtime Slice For The First Milestone

### Required Asset Types

The first end-to-end PSP milestone only needs these cooked/runtime asset types:

- scene asset
- generated cube model asset
- standard material asset, interpreted through a minimal PSP runtime contract

### Required Scene/Runtime Features

The first milestone needs these runtime features to work through the real scene path:

- transform hierarchy from generated core
- camera component
- mesh component
- runtime update path for the authored cube spin script
- scene loading through generated startup and scene-catalog metadata

### Explicitly Deferred Features

The first milestone does not need:

- directional light shading
- shadow rendering
- post-processing
- UI/text rendering
- full scene-switching/menu behavior
- additional city rendering scenes

The authored directional light component may exist in the startup scene and should not block scene load, but it does not contribute to rendering in this milestone.

## Rendering Design

The PSP renderer should walk the live runtime scene and submit the real cube mesh through the PSP GU path.

The initial render path should:

- find the active camera from runtime scene state
- locate mesh drawables produced by the real scene load
- transform the cube vertices through the normal runtime transform path
- submit the cube as an opaque unlit triangle list

The material contract should remain intentionally narrow:

- accept the cooked standard material asset so the real packaged scene can load
- map it to a simple PSP unlit draw path
- use a fixed or trivially derived opaque color for the first milestone

This proves the real asset/runtime contract without blocking on lighting parity.

## Codegen And Script Behavior

The cube rotation must come from the real authored runtime path, not from a PSP-only animation shortcut.

That means:

- generated core must be compiled into the PSP player
- generated runtime/component code needed by the scene must be present
- the authored spin script must execute through the normal update flow

If the broader future code-module residency model is not ready for PSP, the first milestone may treat the required generated gameplay code as startup-resident. What it must not do is bypass generated code entirely.

## Failure Behavior

The PSP path should preserve explicit failure behavior:

- no fallback hardcoded cube scene
- no silent scene replacement
- no default startup scene if runtime metadata is missing
- no empty-frame success state when required scene/model/material inputs fail

Required failures should include:

- missing generated startup scene metadata
- missing cooked startup scene file
- missing required cooked model/material assets
- missing generated core inputs needed to compile the player
- unsupported required runtime data needed to render the startup cube

Unsupported optional rendering behavior, such as directional light contribution, may be ignored for this milestone as long as scene load still succeeds and required mesh rendering remains correct.

## Verification

Success for the first milestone means:

1. the editor recognizes `psp` as an installed build target
2. the `city` project can select PSP through the normal build flow
3. the editor regenerates generated core for `psp`
4. the PSP builder emits a staged homebrew app folder ending in `EBOOT.PBP`
5. PPSSPP launches that `EBOOT.PBP` from the staged app folder
6. the real cooked `cube_test` startup scene loads
7. the cube appears on screen and rotates through the normal runtime/script path
8. no PSP-only hardcoded scene logic is required to achieve the result

## Risks

- PSP native rendering may expose gaps in the generated runtime path that Windows currently hides.
- The first supported standard-material interpretation may need a temporary PSP-specific compatibility rule before a more complete material path exists.
- Gameplay/generated script compilation for PSP may expose assumptions in the code-module pipeline that currently only hold for Windows.
- PSP filesystem startup behavior may require one app-root discovery helper even though the manifest path model stays cooked-relative.

## Recommendation

Implement PSP as a real editor-driven native target with:

- editor-owned generated core regeneration
- builder-owned PSP packaging and native build execution
- Windows-style cooked-relative runtime manifests
- a narrow first renderer that draws the real `cube_test` cube unlit

This is the smallest design that still proves the actual Helengine platform contract instead of a one-off PSP demo path.
