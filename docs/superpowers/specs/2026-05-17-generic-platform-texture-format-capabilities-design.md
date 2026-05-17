# Generic Platform Texture Format Capabilities Design

## Goal

Add one generic platform capability system that lets platforms declare which texture color formats, alpha precisions, and format combinations they support for builder-owned texture cooking. The editor must use that metadata to constrain texture settings for both image textures and font-atlas textures, and builders must keep consuming the resolved generic `TextureAssetProcessorSettings` contract unchanged.

## Problem

The current PSP work-item pipeline executes generic texture settings correctly, but the editor still treats texture processor settings as effectively free-form:

- the importer UI exposes generic `ColorFormat`, `AlphaPrecision`, and `MaxResolution`
- platforms do not declare which combinations are valid
- invalid combinations can be authored even when the target platform cannot consume them
- `texture` and `font-atlas-texture` share the same practical settings needs, but there is no shared capability contract that constrains both

This leaves an important gap between generic editor authoring and platform reality.

## Design Principles

1. Keep texture processor settings generic.
   The existing `TextureAssetProcessorSettings` model remains the source of truth for resolved settings:
   - `MaxResolution`
   - `ColorFormat`
   - `AlphaPrecision`

2. Put platform support metadata on the platform contract.
   The platform definition should advertise what it supports. The editor should not hardcode PSP, DS, or PS2 assumptions.

3. Constrain both images and font atlases through the same system.
   `texture` and `font-atlas-texture` are both texture-processing contracts, so they should use the same generic capability shape.

4. Preserve the work-item pipeline.
   Builders should continue to read editor-resolved settings from serialized work items. This feature should improve authoring and validation, not change the execution contract.

## Recommended Approach

Extend `PlatformAssetCookCapabilityDefinition` so a builder-owned texture capability can optionally publish generic texture-format support metadata:

- supported texture color formats
- supported texture alpha precisions
- supported color-format and alpha-precision combinations

The editor importer UI reads that metadata for the active platform and active asset kind, constrains the available values, and automatically repairs invalid selections when the platform changes or when the user picks a color format that makes the current alpha precision invalid.

## Capability Model

### New Generic Metadata

Add a new generic definition object to the base platform model:

- `PlatformTextureFormatCapabilityDefinition`

This definition should include:

- `TextureAssetColorFormat[] SupportedColorFormats`
- `TextureAssetAlphaPrecision[] SupportedAlphaPrecisions`
- `PlatformTextureFormatCombinationDefinition[] SupportedCombinations`

Add a small combination record:

- `PlatformTextureFormatCombinationDefinition`
  - `TextureAssetColorFormat ColorFormat`
  - `TextureAssetAlphaPrecision AlphaPrecision`

### Where It Lives

Attach this metadata to `PlatformAssetCookCapabilityDefinition` as an optional property:

- `PlatformTextureFormatCapabilityDefinition TextureFormatCapabilities`

This keeps the capability tied to the cook contract that already owns:

- `SourceAssetKind`
- `TargetArtifactKind`
- `OwnershipKind`
- `SettingsContractId`
- `DefaultSerializedPlatformSettings`

### Scope

The editor should consult this metadata only when:

- the capability is builder-owned
- the capability source kind is `texture` or `font-atlas-texture`

If no texture-format capability metadata is present, the editor should keep current unconstrained behavior.

## Editor Behavior

### Asset Import Settings UI

The texture processor section in `AssetImportSettingsView` should become capability-aware.

For the selected platform and selected asset kind:

1. Resolve the matching platform cook capability:
   - `texture` for images
   - `font-atlas-texture` for fonts

2. If the capability includes `TextureFormatCapabilities`:
   - show only supported color formats
   - show only supported alpha precisions
   - enforce supported combinations

3. If the current pending selection is invalid:
   - choose a valid replacement automatically
   - do not leave the UI in an invalid state

4. If no capability metadata exists:
   - keep current generic controls and values

### Combination Repair Rules

When the current combination becomes invalid, the editor should repair it deterministically:

1. Prefer keeping the current color format and changing alpha precision if a valid combination exists.
2. Otherwise choose the first valid combination in platform-declared order.
3. Never emit a combination not published by the active platform capability.

This keeps the behavior predictable and testable.

### Fonts

Font assets must use the same constrained texture-format controls, but through the `font-atlas-texture` capability instead of `texture`.

That means:

- source images use `texture`
- font atlas output uses `font-atlas-texture`
- both use the same generic capability model

Platforms may publish:

- identical capabilities for both
- or different capabilities later if a platform wants font atlases to use a narrower subset

## Builder Behavior

No structural execution change is required in the PSP builder once capability metadata exists.

Builders should continue to:

- consume serialized generic settings from work items
- deserialize `TextureAssetProcessorSettings`
- cook output directly to the declared builder-owned output path

This feature is about:

- editor-side capability advertisement
- authoring constraints
- validity guarantees

not about changing the serialized execution contract.

## PSP Platform Definition

PSP should publish explicit texture-format capability metadata for:

- `texture`
- `font-atlas-texture`

The first PSP version should declare exactly the combinations the runtime can support safely today. The builder defaults can remain:

- `ColorFormat = Rgba4444`
- `AlphaPrecision = A4`
- `MaxResolution = 0`

But those defaults must now sit inside a capability that also tells the editor which other values are legal.

## Validation and Testing

### Base Platform Tests

Add tests for:

- capability definitions storing texture-format metadata
- combination definitions preserving declared order and values

### Editor Tests

Add tests for `AssetImportSettingsView` showing constrained values when platform capabilities are present:

- image texture path uses `texture` capability
- font texture path uses `font-atlas-texture` capability
- invalid selections are auto-repaired to valid combinations
- missing capability metadata falls back to current unconstrained behavior

### PSP Tests

Keep focused PSP tests for:

- capability publication in `PspPlatformDefinitionFactory`
- builder-owned work-item execution using the unchanged generic settings contract

## Risks

1. UI drift between images and fonts.
   Mitigation: route both through the same generic capability resolution path, keyed by asset kind.

2. Overfitting the model to PSP.
   Mitigation: keep the capability model generic and based only on shared enums and generic settings.

3. Invalid auto-repair behavior surprising authors.
   Mitigation: make repair deterministic and test it directly.

## Out of Scope

This design does not include:

- adding PSP-only editor controls
- changing the serialized work-item schema beyond using the existing generic texture settings contract
- changing runtime texture payload formats
- adding platform-specific builder rediscovery logic

## Expected Outcome

After implementation:

- platforms generically advertise supported texture formats and valid color/alpha combinations
- the editor constrains texture settings for both images and font atlases
- PSP work-item execution continues to consume final resolved generic settings
- invalid format combinations are prevented at authoring time instead of surfacing later in builder execution
