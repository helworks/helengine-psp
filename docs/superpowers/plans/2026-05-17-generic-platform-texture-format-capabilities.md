# Generic Platform Texture Format Capabilities Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a generic platform capability system that constrains texture color formats, alpha precisions, and valid combinations for both image textures and font-atlas textures, while keeping PSP builder-owned cook work-item execution on the existing generic `TextureAssetProcessorSettings` contract.

**Architecture:** Extend the base platform capability model with optional texture-format metadata, teach the editor texture importer UI to resolve and enforce that metadata for both `texture` and `font-atlas-texture`, then update the PSP platform definition to publish its supported formats and keep the builder execution path unchanged. Validation starts at the base-model layer, then the editor UI layer, and finally the PSP definition/builder layer.

**Tech Stack:** C#/.NET 9, `helengine.baseplatform`, `helengine.editor`, `helengine.psp.builder`, xUnit, RTK `dotnet test`

---

## File Map

### Base platform contract

- Modify: `C:\dev\helworks\helengine\engine\helengine.baseplatform\Definitions\PlatformAssetCookCapabilityDefinition.cs`
  - Add optional generic texture-format capability metadata to the cook capability model.
- Create: `C:\dev\helworks\helengine\engine\helengine.baseplatform\Definitions\PlatformTextureFormatCapabilityDefinition.cs`
  - Define supported color formats, supported alpha precisions, and supported combinations.
- Create: `C:\dev\helworks\helengine\engine\helengine.baseplatform\Definitions\PlatformTextureFormatCombinationDefinition.cs`
  - Define one valid color-format and alpha-precision pair.
- Modify: `C:\dev\helworks\helengine\engine\helengine.baseplatform.tests\Builders\IPlatformAssetBuilderMetadataTests.cs`
  - Add tests for the new metadata surface on published capabilities.
- Create or modify: `C:\dev\helworks\helengine\engine\helengine.baseplatform.tests\Definitions\...`
  - Add focused unit tests for the new capability and combination definitions if a dedicated definitions test file exists or create one if it does not.

### Editor capability-aware UI and settings flow

- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\components\ui\AssetImportSettingsView.cs`
  - Resolve active platform texture capability metadata and constrain image/font texture controls.
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\EditorSession.cs`
  - Pass platform capability metadata into the asset settings view if that plumbing does not already exist through current parameters.
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\AssetImportSettingsViewTests.cs`
  - Add tests for constrained color formats, constrained alpha precisions, valid-combination repair, and fallback behavior.
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\EditorWindowsBuildScenePackagerTests.cs`
  - Add at least one packaging/work-item test proving font-atlas texture capability metadata is resolved from the generic platform contract if needed.

### PSP platform publication and regression coverage

- Modify: `C:\dev\helworks\helengine-psp\builder\PspPlatformDefinitionFactory.cs`
  - Publish PSP-supported texture formats and valid combinations for both `texture` and `font-atlas-texture`.
- Modify: `C:\dev\helworks\helengine-psp\builder.tests\PspPlatformAssetBuilderTests.cs`
  - Extend metadata assertions to verify PSP capability publication includes the new generic texture-format constraints.
- Modify: `C:\dev\helworks\helengine-psp\builder\PspTextureCookSettingsSerializer.cs`
  - Only if required to align with the final generic defaults/validation model; avoid changing the serialized execution contract unless necessary.

---

### Task 1: Add Base Platform Texture Capability Types

**Files:**
- Create: `C:\dev\helworks\helengine\engine\helengine.baseplatform\Definitions\PlatformTextureFormatCapabilityDefinition.cs`
- Create: `C:\dev\helworks\helengine\engine\helengine.baseplatform\Definitions\PlatformTextureFormatCombinationDefinition.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.baseplatform\Definitions\PlatformAssetCookCapabilityDefinition.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.baseplatform.tests\Builders\IPlatformAssetBuilderMetadataTests.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.baseplatform.tests\Definitions\PlatformTextureFormatCapabilityDefinitionTests.cs` (create if needed)

- [ ] **Step 1: Write failing base-platform tests for texture capability metadata**

Add tests that construct a `PlatformAssetCookCapabilityDefinition` with:
- `SourceAssetKind = "texture"`
- `TargetArtifactKind = "runtime-texture"`
- `OwnershipKind = BuilderOwned`
- one `PlatformTextureFormatCapabilityDefinition` containing:
  - `SupportedColorFormats = [Rgba4444, Indexed8]`
  - `SupportedAlphaPrecisions = [A4, A8]`
  - `SupportedCombinations = [(Rgba4444, A4), (Indexed8, A8)]`

Assertions:
- metadata is stored on the capability
- arrays preserve declared order
- combinations preserve declared values exactly

- [ ] **Step 2: Run the base-platform tests and verify they fail**

Run:
```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.baseplatform.tests\helengine.baseplatform.tests.csproj --filter FullyQualifiedName~TextureFormat -v q -m:1 /nodeReuse:false
```

Expected:
- compile or test failure because the new texture capability types or property do not exist yet

- [ ] **Step 3: Implement the new base-platform definitions**

Implement:
- `PlatformTextureFormatCombinationDefinition`
  - constructor validates enum values are accepted directly
  - expose `ColorFormat` and `AlphaPrecision`
- `PlatformTextureFormatCapabilityDefinition`
  - constructor validates non-null arrays/lists
  - expose `SupportedColorFormats`, `SupportedAlphaPrecisions`, `SupportedCombinations`
- `PlatformAssetCookCapabilityDefinition`
  - add optional `TextureFormatCapabilities` property
  - preserve current constructor overload behavior
  - add a constructor overload or parameter extension without breaking current callers

Follow repo rules:
- one class per file
- XML comments on all members
- no tuples

- [ ] **Step 4: Run the base-platform tests and verify they pass**

Run:
```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.baseplatform.tests\helengine.baseplatform.tests.csproj --filter FullyQualifiedName~TextureFormat -v q -m:1 /nodeReuse:false
```

Expected:
- PASS for the new texture capability tests

- [ ] **Step 5: Commit the base-platform contract change**

```powershell
git add C:\dev\helworks\helengine\engine\helengine.baseplatform\Definitions\PlatformAssetCookCapabilityDefinition.cs C:\dev\helworks\helengine\engine\helengine.baseplatform\Definitions\PlatformTextureFormatCapabilityDefinition.cs C:\dev\helworks\helengine\engine\helengine.baseplatform\Definitions\PlatformTextureFormatCombinationDefinition.cs C:\dev\helworks\helengine\engine\helengine.baseplatform.tests
git commit -m "Add generic platform texture format capability metadata"
```

### Task 2: Make Asset Import Settings UI Capability-Aware

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\components\ui\AssetImportSettingsView.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\EditorSession.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\AssetImportSettingsViewTests.cs`

- [ ] **Step 1: Write failing editor tests for constrained image and font texture settings**

Add tests covering:
- image assets use the `texture` capability
- font assets use the `font-atlas-texture` capability
- unsupported color formats are not offered when capability metadata exists
- unsupported alpha precisions are not offered when capability metadata exists
- invalid color/alpha combinations are repaired deterministically
- absence of capability metadata keeps current unconstrained behavior

Concrete scenarios:
- platform capability allows only `(Rgba4444, A4)` and `(Indexed8, A8)`
- pending settings start with invalid `(Rgba4444, A8)`
- assert the view repairs to `(Rgba4444, A4)` if that preserves color format

- [ ] **Step 2: Run the editor view tests and verify they fail**

Run:
```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~AssetImportSettingsViewTests -v q -m:1 /nodeReuse:false
```

Expected:
- failures because the view does not yet resolve or enforce texture capability metadata

- [ ] **Step 3: Thread platform texture capability metadata into the settings view**

In `EditorSession.cs`:
- identify where `AssetImportSettingsView.Show(...)` is called for image and font entries
- pass the active platform definition or the resolved texture capability map the view needs

In `AssetImportSettingsView.cs`:
- add a small internal resolution path for active asset kind:
  - `texture` when `AssetEntryKind.Image`
  - `font-atlas-texture` when `AssetEntryKind.Font`
- resolve active platform capability metadata
- constrain the color-format and alpha-precision option lists
- implement deterministic invalid-combination repair:
  1. keep color format if possible
  2. otherwise choose the first valid combination in declared order
- preserve current fallback behavior when no metadata exists

Do not add PSP-specific branches.

- [ ] **Step 4: Run the editor view tests and verify they pass**

Run:
```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~AssetImportSettingsViewTests -v q -m:1 /nodeReuse:false
```

Expected:
- PASS for the new capability-aware UI tests

- [ ] **Step 5: Commit the editor UI capability work**

```powershell
git add C:\dev\helworks\helengine\engine\helengine.editor\components\ui\AssetImportSettingsView.cs C:\dev\helworks\helengine\engine\helengine.editor\EditorSession.cs C:\dev\helworks\helengine\engine\helengine.editor.tests\AssetImportSettingsViewTests.cs
git commit -m "Constrain texture import settings by platform capabilities"
```

### Task 3: Keep Editor Packaging and Work-Item Emission Aligned

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\EditorWindowsBuildScenePackagerTests.cs`
- Inspect only if needed: `C:\dev\helworks\helengine\engine\helengine.editor\managers\project\EditorPlatformCookWorkItemFactory.cs`

- [ ] **Step 1: Write one failing packaging test for font-atlas texture capability usage**

Add a test similar to the existing builder-owned texture work-item emission tests, but for a source font or packaged `.hefont` path where the platform publishes `font-atlas-texture` capability metadata.

Assertions:
- one `PlatformCookWorkItem` is emitted
- `SourceAssetKind == "font-atlas-texture"`
- `SerializedPlatformSettings` reflects the expected generic texture settings defaults for the active platform

- [ ] **Step 2: Run the targeted packaging test and verify it fails only if packaging is not already aligned**

Run:
```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~PlatformOwnsImportedDiffuseTextureCooking -v q -m:1 /nodeReuse:false
```

If the new font-atlas test has a unique name, use that exact filter instead.

Expected:
- either fail because the test is new and packaging behavior is incomplete
- or pass immediately, proving no code change is needed in the packager

- [ ] **Step 3: Implement only the minimal editor packaging change if needed**

If the test fails:
- update `EditorPlatformCookWorkItemFactory` only enough to ensure `font-atlas-texture` uses the same generic capability metadata/default settings resolution path as `texture`
- do not fork PSP-specific behavior

If the test already passes:
- leave production code unchanged

- [ ] **Step 4: Run the targeted packaging test and verify it passes**

Run the same command from Step 2.

Expected:
- PASS

- [ ] **Step 5: Commit the packaging-alignment work**

```powershell
git add C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\EditorWindowsBuildScenePackagerTests.cs C:\dev\helworks\helengine\engine\helengine.editor\managers\project\EditorPlatformCookWorkItemFactory.cs
git commit -m "Align editor cook work items with texture format capabilities"
```

### Task 4: Publish PSP Texture Capability Metadata

**Files:**
- Modify: `C:\dev\helworks\helengine-psp\builder\PspPlatformDefinitionFactory.cs`
- Modify: `C:\dev\helworks\helengine-psp\builder.tests\PspPlatformAssetBuilderTests.cs`

- [ ] **Step 1: Write failing PSP metadata tests for texture and font-atlas capability publication**

Extend `PspPlatformAssetBuilderTests` so the PSP definition assertions verify:
- `texture` capability publishes texture-format metadata
- `font-atlas-texture` capability publishes texture-format metadata
- supported formats include the exact PSP-supported values you intend to expose
- supported combinations contain only valid PSP pairs
- default serialized settings still round-trip to the expected PSP defaults

- [ ] **Step 2: Run the focused PSP metadata tests and verify they fail**

Run:
```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~Descriptor_and_definition_expose_psp_metadata -v q -m:1 /nodeReuse:false
```

Expected:
- failure because PSP capability metadata is not yet published

- [ ] **Step 3: Publish PSP texture-format capabilities**

In `PspPlatformDefinitionFactory.cs`:
- build one `PlatformTextureFormatCapabilityDefinition` for `texture`
- build one `PlatformTextureFormatCapabilityDefinition` for `font-atlas-texture`
- include the exact PSP-supported color formats, alpha precisions, and valid combinations
- keep current serialized default settings aligned with one valid published combination

Do not change the builder execution path unless the new metadata model requires minor constructor call updates.

- [ ] **Step 4: Run the focused PSP builder tests and verify they pass**

Run:
```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~PspPlatformAssetBuilderTests -v q -m:1 /nodeReuse:false
```

Expected:
- PASS for the PSP metadata assertions
- PASS for the existing texture/font-atlas work-item execution tests

- [ ] **Step 5: Commit the PSP definition update**

```powershell
git add C:\dev\helworks\helengine-psp\builder\PspPlatformDefinitionFactory.cs C:\dev\helworks\helengine-psp\builder.tests\PspPlatformAssetBuilderTests.cs
git commit -m "Publish PSP texture format capabilities"
```

### Task 5: Final Cross-Layer Verification

**Files:**
- Verify only; no planned code edits unless failures appear

- [ ] **Step 1: Run the base-platform focused suite**

Run:
```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.baseplatform.tests\helengine.baseplatform.tests.csproj --filter FullyQualifiedName~TextureFormat -v q -m:1 /nodeReuse:false
```

Expected:
- PASS

- [ ] **Step 2: Run the editor capability-aware view tests**

Run:
```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~AssetImportSettingsViewTests -v q -m:1 /nodeReuse:false
```

Expected:
- PASS

- [ ] **Step 3: Run the focused PSP builder tests**

Run:
```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~PspPlatformAssetBuilderTests -v q -m:1 /nodeReuse:false
```

Expected:
- PASS

- [ ] **Step 4: Record any warnings that are unrelated to this feature**

Document in the implementation summary:
- any persistent IDE style warnings
- any unrelated build noise from sibling projects

- [ ] **Step 5: Commit any final cleanup**

```powershell
git add .
git commit -m "Finish generic texture format capability support"
```

---

## Notes for the Implementer

- Keep the serialized work-item settings contract generic. Do not introduce PSP-specific fields into `TextureAssetProcessorSettings`.
- Do not add platform-specific branching in the editor UI. The editor must only consume platform-published capability metadata.
- Font assets must resolve through `font-atlas-texture`, not through `texture`.
- Prefer extending current constructors conservatively in `PlatformAssetCookCapabilityDefinition` so existing callers continue to compile with minimal churn.
- Use the smallest test slices necessary while iterating. The full editor suite is too broad for this feature unless a focused test keeps failing unexpectedly.
