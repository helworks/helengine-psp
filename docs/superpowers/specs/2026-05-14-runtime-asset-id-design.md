# Runtime Asset ID Design

**Goal**

Introduce a deterministic numeric runtime asset identity for all cooked assets so player runtimes can match and reuse packaged assets without relying on editor-facing string IDs.

**Problem**

The PSP return-to-menu crash exposed a weak content identity contract. Runtime texture reuse on PSP currently depends on `TextureAsset.Id` being a non-empty string. Packaged font atlas textures lose identity during cooking and deserialization, so the runtime rebuilds duplicate native textures on reload instead of reusing the same packaged asset identity. Windows tolerates this because it has more memory headroom, but the contract is still wrong there.

**Decision**

- Keep human-readable asset identity in tooling as a canonical string key.
- Add `RuntimeAssetId` as a `ulong` on the base `Asset` type for all cooked assets.
- Compute `RuntimeAssetId` during build/cook from a canonical string identity.
- Serialize the numeric `RuntimeAssetId` into cooked assets and cooked subresources.
- Use `RuntimeAssetId` in player/runtime caches and runtime ownership maps.
- Treat `RuntimeAssetId == 0` as ephemeral runtime-only content that is not eligible for packaged-asset reuse.

**Canonical Identity Model**

- Imported cooked assets use their cooked logical path as the canonical key.
- Embedded packaged subresources use a deterministic derived key from the owning cooked asset.
- Example cooked asset key: `cooked/Fonts/DemoDiscBody.hefont`
- Example font atlas subresource key: `cooked/Fonts/DemoDiscBody.hefont#atlas`

The canonical key is not the runtime comparison surface. It is the build-time source of truth used to derive the numeric runtime identity.

**Hashing Contract**

- The cook step hashes the canonical key into `RuntimeAssetId`.
- The runtime does not hash canonical keys during player execution.
- The hash must be deterministic across editor builds.
- The build should fail if two distinct canonical keys map to the same `RuntimeAssetId`.
- Runtime does not attempt collision detection or recovery.

**Type Choice**

- Use `ulong` / `uint64` for `RuntimeAssetId`.
- `uint32` is not acceptable as the primary engine-wide runtime asset identity because collision risk is too high for a silent runtime cache key.

**Migration Shape**

- Add `RuntimeAssetId` to the base `Asset` type now.
- Keep the existing string `Id` temporarily for editor-facing workflows and existing tooling.
- Move runtime cache lookups away from string `Id` and onto `RuntimeAssetId`.
- Extend cooked serializers and deserializers to persist `RuntimeAssetId`.
- Recook all `city` content after the change.
- Do not preserve backward compatibility for previously cooked `city` outputs.

**Windows And PSP Behavior**

- Windows and PSP should share the same runtime asset identity contract.
- Windows may not currently fail, but it should still stop depending on editor string IDs for runtime asset reuse.
- PSP becomes the first concrete validation target because it is currently failing on the missing identity contract.

**Non-Goals**

- Do not invent random runtime IDs at load time.
- Do not keep string-based matching in platform caches as the long-term contract.
- Do not preserve previously cooked `city` assets.

**First Validation Target**

The first runtime scenario that should prove the new contract is:

1. boot `DemoDiscMainMenu`
2. open `cube_test`
3. press `Circle`
4. return cleanly to `DemoDiscMainMenu`

That path currently fails because the font atlas behind `DemoDiscBody.hefont` loses stable runtime identity between loads.
