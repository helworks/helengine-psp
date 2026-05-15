# Runtime Asset ID Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add deterministic numeric runtime asset IDs to all cooked assets and switch player runtime matching to those IDs so PSP and Windows can reuse packaged assets consistently.

**Architecture:** The editor and cook pipeline remain the source of readable canonical asset keys. Cooking derives a `ulong RuntimeAssetId` from those keys, serializers persist it into cooked assets and cooked subresources, and runtime/platform caches match only on the numeric ID. The first concrete validation target is the PSP font-atlas reload path that currently loses identity across `DemoDiscMainMenu -> cube_test -> DemoDiscMainMenu`.

**Tech Stack:** C# (.NET 9 engine/editor/build tooling), C++ PSP runtime cache code, xUnit test projects, editor CLI export flow, PPSSPP boot verification.

---

## File Structure

- `C:\dev\helworks\helengine\engine\helengine.core\assets\Asset.cs`
  Base runtime asset contract; add `RuntimeAssetId`.
- `C:\dev\helworks\helengine\engine\helengine.core\assets\raw\TextureAsset.cs`
  Inherits the base contract; runtime texture cache consumers will read numeric identity from here.
- `C:\dev\helworks\helengine\engine\helengine.core\serialization\EngineBinaryReader.cs`
  Add `ReadUInt64()`.
- `C:\dev\helworks\helengine\engine\helengine.files\serialization\EngineBinaryWriter.cs`
  Add `WriteUInt64(ulong)`.
- `C:\dev\helworks\helengine\engine\helengine.editor\utils\RuntimeAssetIdGenerator.cs`
  New deterministic canonical-key hashing helper for cook-time use.
- `C:\dev\helworks\helengine\engine\helengine.editor.tests\utils\RuntimeAssetIdGeneratorTests.cs`
  Hash determinism and collision-oriented guardrails.
- `C:\dev\helworks\helengine\engine\helengine.editor.windows\content\font\GDIFontProcessor.cs`
  Assign a deterministic atlas `RuntimeAssetId` for cooked font atlases.
- `C:\dev\helworks\helengine\engine\helengine.files\assets\font\FontAssetBinarySerializer.cs`
  Serialize font atlas `RuntimeAssetId`.
- `C:\dev\helworks\helengine\engine\helengine.core\assets\font\FontAssetBinarySerializer.cs`
  Deserialize font atlas `RuntimeAssetId`.
- `C:\dev\helworks\helengine\engine\helengine.editor\managers\asset\TextureAssetProcessor.cs`
  Assign base cooked runtime IDs to normal texture assets.
- `C:\dev\helworks\helengine\engine\helengine.files\assets\AssetSerializer.cs`
  Update generic asset serialization flow if base asset IDs need to be written centrally.
- `C:\dev\helworks\helengine\engine\helengine.core\assets\AssetSerializer.cs`
  Update generic asset deserialization flow if base asset IDs need to be read centrally.
- `C:\dev\helworks\helengine\engine\helengine.editor.tests\serialization\FontAssetBinarySerializerTests.cs`
  Verify cooked font atlases round-trip runtime IDs.
- `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\asset\TextureAssetProcessorTests.cs`
  Verify cooked texture runtime IDs are deterministic and path-derived.
- `C:\dev\helworks\helengine\engine\helengine.editor.tests\serialization\scene\SceneManagerTests.cs`
  Extend reload coverage to assert packaged font textures keep the same runtime identity across scene reload.
- `C:\dev\helworks\helengine-psp\src\platform\psp\rendering\PspTextureCache.cpp`
  Switch cache matching from string `Id` to `RuntimeAssetId`.
- `C:\dev\helworks\helengine-psp\src\platform\psp\rendering\PspTextureCache.hpp`
  Signature updates and field naming alignment.
- `C:\dev\helworks\helengine-psp\builder.tests\...` and existing PSP boot scripts
  Used only for final verification, not for the core identity contract itself.

### Task 1: Add the engine-wide runtime asset ID contract

**Files:**
- Create: `C:\dev\helworks\helengine\engine\helengine.editor\utils\RuntimeAssetIdGenerator.cs`
- Create: `C:\dev\helworks\helengine\engine\helengine.editor.tests\utils\RuntimeAssetIdGeneratorTests.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.core\assets\Asset.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.core\serialization\EngineBinaryReader.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.files\serialization\EngineBinaryWriter.cs`

- [ ] **Step 1: Write the failing tests**

```csharp
namespace helengine.editor.tests {
    /// <summary>
    /// Verifies deterministic numeric runtime asset id generation.
    /// </summary>
    public sealed class RuntimeAssetIdGeneratorTests {
        /// <summary>
        /// Ensures the same canonical key always hashes to the same runtime id.
        /// </summary>
        [Fact]
        public void Generate_whenCanonicalKeyMatches_returnsSameId() {
            RuntimeAssetIdGenerator generator = new RuntimeAssetIdGenerator();

            ulong first = generator.Generate("cooked/Fonts/DemoDiscBody.hefont#atlas");
            ulong second = generator.Generate("cooked/Fonts/DemoDiscBody.hefont#atlas");

            Assert.Equal(first, second);
            Assert.NotEqual(0ul, first);
        }

        /// <summary>
        /// Ensures equivalent path formats normalize to one runtime identity.
        /// </summary>
        [Fact]
        public void Generate_whenPathSeparatorsDiffer_returnsSameId() {
            RuntimeAssetIdGenerator generator = new RuntimeAssetIdGenerator();

            ulong unixStyle = generator.Generate("cooked/Fonts/DemoDiscBody.hefont#atlas");
            ulong windowsStyle = generator.Generate("cooked\\Fonts\\DemoDiscBody.hefont#atlas");

            Assert.Equal(unixStyle, windowsStyle);
        }
    }
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~RuntimeAssetIdGeneratorTests -v minimal
```

Expected:

```text
FAIL ... RuntimeAssetIdGeneratorTests
error CS0246: The type or namespace name 'RuntimeAssetIdGenerator' could not be found
```

- [ ] **Step 3: Add the base contract and hash utility**

```csharp
namespace helengine {
    /// <summary>
    /// Base asset type containing editor-facing and runtime-facing identities.
    /// </summary>
    public class Asset {
        /// <summary>
        /// Gets or sets the editor-facing asset identifier.
        /// </summary>
        public string Id { get; set; }

        /// <summary>
        /// Gets or sets the deterministic cooked runtime asset identifier.
        /// A value of zero indicates ephemeral runtime-only content.
        /// </summary>
        public ulong RuntimeAssetId { get; set; }
    }
}
```

```csharp
using System.Security.Cryptography;
using System.Text;

namespace helengine.editor {
    /// <summary>
    /// Generates deterministic numeric runtime asset identifiers from canonical asset keys.
    /// </summary>
    public sealed class RuntimeAssetIdGenerator {
        /// <summary>
        /// Generates a runtime asset id from the supplied canonical key.
        /// </summary>
        /// <param name="canonicalKey">Cooked logical asset key.</param>
        /// <returns>Deterministic non-zero runtime asset id.</returns>
        public ulong Generate(string canonicalKey) {
            if (string.IsNullOrWhiteSpace(canonicalKey)) {
                throw new ArgumentException("Canonical asset key must be provided.", nameof(canonicalKey));
            }

            string normalized = canonicalKey.Replace('\\', '/').ToLowerInvariant();
            byte[] bytes = Encoding.UTF8.GetBytes(normalized);
            byte[] hash = SHA256.HashData(bytes);
            ulong value = BitConverter.ToUInt64(hash, 0);
            return value == 0ul ? 1ul : value;
        }
    }
}
```

```csharp
public abstract ulong ReadUInt64();
public abstract void WriteUInt64(ulong value);
```

- [ ] **Step 4: Run the focused tests to verify they pass**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~RuntimeAssetIdGeneratorTests -v minimal
```

Expected:

```text
Passed! 2 tests passed.
```

- [ ] **Step 5: Commit**

```bash
git -C C:\dev\helworks\helengine add engine/helengine.core/assets/Asset.cs engine/helengine.core/serialization/EngineBinaryReader.cs engine/helengine.files/serialization/EngineBinaryWriter.cs engine/helengine.editor/utils/RuntimeAssetIdGenerator.cs engine/helengine.editor.tests/utils/RuntimeAssetIdGeneratorTests.cs
git -C C:\dev\helworks\helengine commit -m "Add runtime asset id contract"
```

### Task 2: Serialize runtime IDs for generic cooked assets and cooked textures

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\managers\asset\TextureAssetProcessor.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.files\assets\AssetSerializer.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.core\assets\AssetSerializer.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\asset\TextureAssetProcessorTests.cs`

- [ ] **Step 1: Write the failing texture processor test**

```csharp
[Fact]
public void ProcessTexture_whenCookedPathMatches_assignsDeterministicRuntimeAssetId() {
    TextureAssetProcessor processor = new TextureAssetProcessor();

    TextureAsset asset = new TextureAsset();
    TextureAsset processed = processor.AssignRuntimeIdentity(asset, "cooked/Textures/ui/button.htex");

    Assert.NotEqual(0ul, processed.RuntimeAssetId);
    Assert.Equal(processed.RuntimeAssetId, processor.AssignRuntimeIdentity(new TextureAsset(), "cooked/Textures/ui/button.htex").RuntimeAssetId);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~ProcessTexture_whenCookedPathMatches_assignsDeterministicRuntimeAssetId -v minimal
```

Expected:

```text
FAIL ... AssignRuntimeIdentity could not be found
```

- [ ] **Step 3: Implement cook-time runtime identity assignment and generic serialization**

```csharp
public sealed class TextureAssetProcessor {
    readonly RuntimeAssetIdGenerator RuntimeAssetIdGenerator = new RuntimeAssetIdGenerator();

    public TextureAsset AssignRuntimeIdentity(TextureAsset asset, string cookedAssetPath) {
        if (asset == null) {
            throw new ArgumentNullException(nameof(asset));
        } else if (string.IsNullOrWhiteSpace(cookedAssetPath)) {
            throw new ArgumentException("Cooked asset path must be provided.", nameof(cookedAssetPath));
        }

        asset.RuntimeAssetId = RuntimeAssetIdGenerator.Generate(cookedAssetPath);
        return asset;
    }
}
```

```csharp
// files AssetSerializer
writer.WriteUInt64(asset.RuntimeAssetId);
...
asset.RuntimeAssetId = reader.ReadUInt64();
```

- [ ] **Step 4: Run the focused tests to verify they pass**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~TextureAssetProcessorTests -v minimal
```

Expected:

```text
Passed! ... TextureAssetProcessorTests
```

- [ ] **Step 5: Commit**

```bash
git -C C:\dev\helworks\helengine add engine/helengine.editor/managers/asset/TextureAssetProcessor.cs engine/helengine.files/assets/AssetSerializer.cs engine/helengine.core/assets/AssetSerializer.cs engine/helengine.editor.tests/managers/asset/TextureAssetProcessorTests.cs
git -C C:\dev\helworks\helengine commit -m "Serialize runtime asset ids for cooked assets"
```

### Task 3: Persist font atlas runtime identity through `.hefont`

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.windows\content\font\GDIFontProcessor.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.files\assets\font\FontAssetBinarySerializer.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.core\assets\font\FontAssetBinarySerializer.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\serialization\FontAssetBinarySerializerTests.cs`

- [ ] **Step 1: Write the failing font serializer round-trip test**

```csharp
[Fact]
public void SerializeDeserialize_whenFontAtlasIsCooked_preservesSourceTextureRuntimeAssetId() {
    FontAsset asset = BuildFontAsset("cooked/Fonts/DemoDiscBody.hefont", 42ul);

    byte[] bytes = helengine.files.FontAssetBinarySerializer.SerializeToBytes(asset);
    FontAsset roundTripped = helengine.files.FontAssetBinarySerializer.Deserialize(new MemoryStream(bytes));

    Assert.Equal(42ul, roundTripped.SourceTextureAsset.RuntimeAssetId);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~preservesSourceTextureRuntimeAssetId -v minimal
```

Expected:

```text
FAIL ... Expected: 42 Actual: 0
```

- [ ] **Step 3: Implement atlas runtime identity generation and font binary persistence**

```csharp
// GDIFontProcessor
RuntimeAssetIdGenerator runtimeAssetIdGenerator = new RuntimeAssetIdGenerator();
rawTex.RuntimeAssetId = runtimeAssetIdGenerator.Generate($"{fontAssetPath}#atlas");
```

```csharp
// helengine.files FontAssetBinarySerializer
writer.WriteUInt64(asset.SourceTextureAsset.RuntimeAssetId);
writer.WriteUInt16(asset.SourceTextureAsset.Width);
writer.WriteUInt16(asset.SourceTextureAsset.Height);
writer.WriteByteArray(asset.SourceTextureAsset.Colors);
```

```csharp
// helengine.core FontAssetBinarySerializer
TextureAsset sourceTexture = new TextureAsset {
    RuntimeAssetId = reader.ReadUInt64(),
    Width = reader.ReadUInt16(),
    Height = reader.ReadUInt16()
};
```

- [ ] **Step 4: Run the focused tests to verify they pass**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~FontAssetBinarySerializerTests -v minimal
```

Expected:

```text
Passed! ... FontAssetBinarySerializerTests
```

- [ ] **Step 5: Commit**

```bash
git -C C:\dev\helworks\helengine add engine/helengine.editor.windows/content/font/GDIFontProcessor.cs engine/helengine.files/assets/font/FontAssetBinarySerializer.cs engine/helengine.core/assets/font/FontAssetBinarySerializer.cs engine/helengine.editor.tests/serialization/FontAssetBinarySerializerTests.cs
git -C C:\dev\helworks\helengine commit -m "Preserve runtime asset ids in cooked fonts"
```

### Task 4: Move PSP runtime texture reuse to numeric runtime IDs

**Files:**
- Modify: `C:\dev\helworks\helengine-psp\src\platform\psp\rendering\PspTextureCache.hpp`
- Modify: `C:\dev\helworks\helengine-psp\src\platform\psp\rendering\PspTextureCache.cpp`
- Test: reuse existing PPSSPP boot scripts and PSP scene reload repro flow

- [ ] **Step 1: Write the failing PSP-side cache expectation as a targeted C++ regression comment/test note**

```cpp
// Expected behavior after this task:
// BuildTextureFromRaw must reuse an existing cached runtime texture when
// TextureAsset.RuntimeAssetId matches a previously loaded packaged texture.
// A zero RuntimeAssetId remains uncached ephemeral content.
```

- [ ] **Step 2: Verify the current runtime still keys off string identity**

Run:

```powershell
rtk rg -n "Id" src/platform/psp/rendering/PspTextureCache.cpp src/platform/psp/rendering/PspTextureCache.hpp
```

Expected:

```text
... cache logic still references TextureAsset->Id ...
```

- [ ] **Step 3: Switch cache matching to `RuntimeAssetId`**

```cpp
uint64_t runtimeAssetId = textureAsset->get_RuntimeAssetId();
if (runtimeAssetId != 0ull) {
    auto existing = RuntimeTextures.find(runtimeAssetId);
    if (existing != RuntimeTextures.end()) {
        return existing->second;
    }
}

PspRuntimeTexture* created = CreateTexture(textureAsset);
if (runtimeAssetId != 0ull) {
    RuntimeTextures[runtimeAssetId] = created;
}
```

- [ ] **Step 4: Rebuild the PSP package and verify the return path**

Run:

```powershell
rtk dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city\project.heproj --build psp --output C:\dev\helprojs\output\psp-platform-info-startup
rtk powershell -File C:\dev\helworks\helengine-psp\tools\install_psp_output_to_ppsspp.ps1 -SourceRoot C:\dev\helprojs\output\psp-platform-info-startup
rtk powershell -File C:\dev\helworks\helengine-psp\tools\run_ppsspp_boot_check.ps1
```

Expected:

```text
LoadAsset success=ms0:/PSP/GAME/HELENGINE/cooked/scenes/DemoDiscMainMenu.hasset
Stage complete RuntimeMainLoop
```

Manual expected result:

```text
DemoDiscMainMenu -> cube_test -> Circle returns to DemoDiscMainMenu without freeze or fatal error.
```

- [ ] **Step 5: Commit**

```bash
git -C C:\dev\helworks\helengine-psp add src/platform/psp/rendering/PspTextureCache.hpp src/platform/psp/rendering/PspTextureCache.cpp
git -C C:\dev\helworks\helengine-psp commit -m "Use runtime asset ids for PSP texture caching"
```

### Task 5: Recook city and run the cross-platform regression gates

**Files:**
- Modify: `C:\dev\helprojs\city\user_settings\build_config.json` only if the current PSP menu selection needs to stay pointed at `DemoDiscMainMenu`
- Verify: `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`

- [ ] **Step 1: Run the engine unit and scene reload gates**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~SceneManagerTests -v minimal
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~FontAssetBinarySerializerTests -v minimal
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~RuntimeAssetIdGeneratorTests -v minimal
```

Expected:

```text
Passed! ... SceneManagerTests
Passed! ... FontAssetBinarySerializerTests
Passed! ... RuntimeAssetIdGeneratorTests
```

- [ ] **Step 2: Recook `city` and rebuild the PSP export**

Run:

```powershell
rtk dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city\project.heproj --build psp --output C:\dev\helprojs\output\psp-platform-info-startup
```

Expected:

```text
Build completed successfully.
```

- [ ] **Step 3: Install to PPSSPP and run the boot check**

Run:

```powershell
rtk powershell -File C:\dev\helworks\helengine-psp\tools\install_psp_output_to_ppsspp.ps1 -SourceRoot C:\dev\helprojs\output\psp-platform-info-startup
rtk powershell -File C:\dev\helworks\helengine-psp\tools\run_ppsspp_boot_check.ps1
```

Expected:

```text
Runtime platform info resolved to 'psp' version '1.0.0'.
LoadAsset success=ms0:/PSP/GAME/HELENGINE/cooked/scenes/DemoDiscMainMenu.hasset
Stage complete RuntimeMainLoop
```

- [ ] **Step 4: Perform the manual PSP regression**

Run:

```text
1. Launch PPSSPP visibly with the latest EBOOT.PBP.
2. Open Cube Test from DemoDiscMainMenu.
3. Press Circle to return.
4. Confirm DemoDiscMainMenu reloads without freeze or fatal error.
```

Expected:

```text
Scene return succeeds and the menu remains interactive after the reload.
```

- [ ] **Step 5: Commit**

```bash
git -C C:\dev\helworks\helengine commit -m "Regenerate cooked runtime asset identities"
git -C C:\dev\helworks\helengine-psp commit --allow-empty -m "Verify PSP runtime asset id reload path"
```

## Self-Review

- Spec coverage:
  - engine-wide numeric identity on base `Asset`: covered by Task 1
  - deterministic hashing and canonical keys: covered by Task 1
  - cooked serialization/deserialization: covered by Tasks 2 and 3
  - font atlas subresource identity: covered by Task 3
  - runtime/player use of numeric IDs: covered by Task 4
  - `city` recook and PSP validation: covered by Task 5
- Placeholder scan:
  - removed `TODO`-style placeholders
  - each task includes explicit files, commands, and expected outcomes
- Type consistency:
  - runtime identity type is `ulong` throughout the plan
  - canonical key generation stays build-side only
  - `RuntimeAssetId == 0` is reserved for ephemeral runtime-only content
