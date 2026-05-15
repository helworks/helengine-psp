# PSP Dynamic Batching Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add engine-side PSP dynamic batching for compatible fixed-function 3D drawables, controlled by a `psp-forward` graphics setting that defaults to `true`.

**Architecture:** The PSP builder will expose and persist a new graphics option, then embed the resolved value into generated runtime manifest data. The PSP boot host will read that generated setting at startup and pass it into `PspRenderManager3D`, which will batch compatible dynamic fixed-function drawables into one transient vertex stream per compatibility bucket while preserving the existing fallback path.

**Tech Stack:** C#, C++, xUnit, PSP GU fixed-function rendering, headless editor build, PPSSPP boot-log profiling

---

### Task 1: Add the PSP Graphics Setting and Build-Side Coverage

**Files:**
- Modify: `builder/PspPlatformDefinitionFactory.cs`
- Modify: `builder.tests/PspPlatformAssetBuilderTests.cs`

- [ ] **Step 1: Write the failing builder metadata test**

Add this test near the existing metadata/profile assertions in `builder.tests/PspPlatformAssetBuilderTests.cs`:

```csharp
/// <summary>
/// Verifies the PSP forward graphics profile exposes dynamic batching as an engine-owned runtime toggle.
/// </summary>
[Fact]
public void Descriptor_and_definition_expose_dynamic_batching_graphics_setting() {
    PspPlatformAssetBuilder builder = new();

    PlatformGraphicsProfileDefinition graphicsProfile = Assert.Single(
        builder.Definition.GraphicsProfiles,
        profile => profile.ProfileId == "psp-forward");

    PlatformSettingDefinition setting = Assert.Single(
        graphicsProfile.Settings,
        profileSetting => profileSetting.SettingId == "dynamic-batching-enabled");

    Assert.Equal("Dynamic Batching Enabled", setting.DisplayName);
    Assert.Equal(PlatformSettingKind.Boolean, setting.Kind);
    Assert.Equal("true", setting.DefaultValue);
    Assert.True(setting.Required);
}
```

- [ ] **Step 2: Run the focused builder test to verify it fails**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter Descriptor_and_definition_expose_dynamic_batching_graphics_setting -v minimal`

Expected: FAIL because `psp-forward` does not yet expose `dynamic-batching-enabled`.

- [ ] **Step 3: Add the graphics setting to the PSP forward profile**

In `builder/PspPlatformDefinitionFactory.cs`, extend the `psp-forward` settings array with this new option after `fullscreen-enabled`:

```csharp
new PlatformSettingDefinition(
    "dynamic-batching-enabled",
    "Dynamic Batching Enabled",
    PlatformSettingKind.Boolean,
    "true",
    true,
    [])
```

- [ ] **Step 4: Run the focused builder test to verify it passes**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter Descriptor_and_definition_expose_dynamic_batching_graphics_setting -v minimal`

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add builder/PspPlatformDefinitionFactory.cs builder.tests/PspPlatformAssetBuilderTests.cs
git commit -m "Add PSP dynamic batching graphics setting"
```

### Task 2: Embed the Resolved Graphics Setting Into Generated PSP Runtime Metadata

**Files:**
- Modify: `builder/PspRuntimeNativeManifestWriter.cs`
- Modify: `builder/PspPlatformAssetBuilder.cs`
- Test: `builder.tests/PspPlatformAssetBuilderTests.cs`

- [ ] **Step 1: Write the failing runtime-manifest test**

Add a new test alongside the existing runtime startup manifest test in `builder.tests/PspPlatformAssetBuilderTests.cs`:

```csharp
/// <summary>
/// Verifies the PSP runtime startup manifest embeds the resolved dynamic batching graphics setting.
/// </summary>
[Fact]
public async Task BuildAsync_rewrites_runtime_startup_manifest_with_dynamic_batching_setting() {
    string workingRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
    string outputRoot = Path.Combine(workingRoot, "out");
    string sourceRoot = Path.Combine(workingRoot, "staging");
    string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
    string generatedCoreRuntimeRoot = Path.Combine(generatedCoreRoot, "runtime");
    string repositoryRoot = Path.Combine(workingRoot, "repository");
    string sceneSourcePath = Path.Combine(sourceRoot, "cooked", "scenes", "rendering", "colored_cube_grid.hasset");

    Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath)!);
    Directory.CreateDirectory(generatedCoreRuntimeRoot);
    Directory.CreateDirectory(Path.Combine(repositoryRoot, "src", "platform", "psp"));

    File.WriteAllText(sceneSourcePath, "scene payload");
    File.WriteAllText(Path.Combine(generatedCoreRoot, "helengine_core_amalgamated.cpp"), "// generated");
    File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "#pragma once");
    File.WriteAllText(Path.Combine(generatedCoreRuntimeRoot, "runtime_startup_manifest.cpp"), "// stale startup");
    File.WriteAllText(Path.Combine(generatedCoreRuntimeRoot, "runtime_scene_catalog_manifest.cpp"), "// stale scene catalog");
    File.WriteAllText(Path.Combine(repositoryRoot, "Makefile"), "# fake");
    File.WriteAllText(Path.Combine(repositoryRoot, "src", "platform", "psp", "PspBootHost.cpp"), "// fake");

    string previousDirectory = Directory.GetCurrentDirectory();
    string previousRepositoryRoot = Environment.GetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT");
    try {
        Directory.SetCurrentDirectory(sourceRoot);
        Environment.SetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT", repositoryRoot);

        PlatformBuildManifest manifest = new(
            3,
            "city",
            "1.0.0",
            "1.0.0",
            "psp",
            "1.0.0",
            "scenes/rendering/colored_cube_grid.helen",
            [
                new PlatformBuildScene(
                    "scenes/rendering/colored_cube_grid.helen",
                    "Colored Cube Grid",
                    "cooked/scenes/rendering/colored_cube_grid.hasset",
                    [],
                    [new KeyValuePair<string, string>("cooked-relative-path", "cooked/scenes/rendering/colored_cube_grid.hasset")])
            ],
            Array.Empty<PlatformBuildAsset>(),
            [
                new PlatformBuildArtifact("cooked/scenes/rendering/colored_cube_grid.hasset", "scene:colored-cube-grid", "sha256:scene", "scene", "shared")
            ],
            Array.Empty<PlatformBuildCodeModule>(),
            Array.Empty<PlatformArtifactPlacement>(),
            new PlatformContainerWritePlan("psp-homebrew", Array.Empty<PlatformContainerArtifact>()));

        PlatformBuildRequest request = new(
            manifest,
            [new PlatformBuildTargetVariant("psp-debug", "psp", "psp", "default")],
            [new PlatformCookProfile(
                "default",
                "Default",
                new PlatformCookProfileCapabilities(
                    "psp",
                    "raw",
                    "rgba",
                    "psp-scene-v1",
                    PlatformSerializationEndianness.LittleEndian))],
            outputRoot,
            Path.Combine(workingRoot, "tmp"),
            selectedBuildProfileId: "debug",
            selectedGraphicsProfileId: "psp-forward",
            selectedCodegenProfileId: "default",
            selectedBuildOptionValues: new Dictionary<string, string>(),
            selectedGraphicsOptionValues: new Dictionary<string, string> {
                ["dynamic-batching-enabled"] = "false"
            },
            selectedCodegenOptionValues: new Dictionary<string, string>(),
            generatedCoreCppRootPath: generatedCoreRoot,
            selectedMediaProfileId: "psp-game-folder",
            selectedStorageProfileId: "homebrew-app");

        PspPlatformAssetBuilder builder = new(new FakePspNativeBuildExecutor());

        await builder.BuildAsync(
            request,
            new RecordingProgressReporter(),
            new RecordingDiagnosticReporter(),
            CancellationToken.None);

        string startupManifestContents = File.ReadAllText(Path.Combine(generatedCoreRuntimeRoot, "runtime_startup_manifest.cpp"));
        Assert.Contains("static const bool kRuntimeDynamicBatchingEnabled = false;", startupManifestContents);
    } finally {
        try {
            Directory.SetCurrentDirectory(previousDirectory);
        } catch {
        }

        try {
            Environment.SetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT", previousRepositoryRoot);
        } catch {
        }

        try {
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, recursive: true);
            }
        } catch {
        }
    }
}
```

- [ ] **Step 2: Run the focused manifest test to verify it fails**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter BuildAsync_rewrites_runtime_startup_manifest_with_dynamic_batching_setting -v minimal`

Expected: FAIL because the generated startup manifest does not currently embed any batching flag.

- [ ] **Step 3: Extend the runtime startup manifest writer API**

In `builder/PspRuntimeNativeManifestWriter.cs`, change the public write signature to accept the selected graphics option map:

```csharp
public void Write(
    string generatedCoreRootPath,
    PlatformBuildManifest manifest,
    IReadOnlyDictionary<string, string> selectedGraphicsOptionValues) {
    if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
        throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
    } else if (manifest == null) {
        throw new ArgumentNullException(nameof(manifest));
    } else if (selectedGraphicsOptionValues == null) {
        throw new ArgumentNullException(nameof(selectedGraphicsOptionValues));
    }

    string runtimeRootPath = Path.Combine(generatedCoreRootPath, "runtime");
    Directory.CreateDirectory(runtimeRootPath);

    File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_startup_manifest.hpp"), BuildStartupManifestHeaderContents());
    File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_startup_manifest.cpp"), BuildStartupManifestSourceContents(manifest, selectedGraphicsOptionValues));
    File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_scene_catalog_manifest.hpp"), BuildSceneCatalogManifestHeaderContents());
    File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_scene_catalog_manifest.cpp"), BuildSceneCatalogManifestSourceContents(manifest));
}
```

- [ ] **Step 4: Add generated C++ accessors for the new flag**

In `builder/PspRuntimeNativeManifestWriter.cs`, extend the generated startup header and source:

```csharp
builder.AppendLine("bool he_get_runtime_dynamic_batching_enabled();");
```

And in `BuildStartupManifestSourceContents`:

```csharp
static string BuildStartupManifestSourceContents(
    PlatformBuildManifest manifest,
    IReadOnlyDictionary<string, string> selectedGraphicsOptionValues) {
    if (manifest == null) {
        throw new ArgumentNullException(nameof(manifest));
    } else if (selectedGraphicsOptionValues == null) {
        throw new ArgumentNullException(nameof(selectedGraphicsOptionValues));
    }

    string startupSceneRelativePath = ResolveStartupSceneRelativePath(manifest);
    bool dynamicBatchingEnabled = ReadBooleanOption(selectedGraphicsOptionValues, "dynamic-batching-enabled", true);
    StringBuilder builder = new();
    builder.AppendLine("#include \"runtime/runtime_startup_manifest.hpp\"");
    builder.AppendLine();
    builder.AppendLine("static const char kRuntimeStartupSceneRelativePath[] = \"" + EscapeCppStringLiteral(startupSceneRelativePath) + "\";");
    builder.AppendLine("static const char kRuntimePlatformName[] = \"" + EscapeCppStringLiteral(manifest.PlatformName) + "\";");
    builder.AppendLine("static const char kRuntimePlatformVersion[] = \"" + EscapeCppStringLiteral(manifest.PlatformVersion) + "\";");
    builder.AppendLine("static const bool kRuntimeDynamicBatchingEnabled = " + (dynamicBatchingEnabled ? "true" : "false") + ";");
    builder.AppendLine();
    builder.AppendLine("const char* he_get_runtime_startup_scene_relative_path() {");
    builder.AppendLine("    return kRuntimeStartupSceneRelativePath;");
    builder.AppendLine("}");
    builder.AppendLine();
    builder.AppendLine("const char* he_get_runtime_platform_name() {");
    builder.AppendLine("    return kRuntimePlatformName;");
    builder.AppendLine("}");
    builder.AppendLine();
    builder.AppendLine("const char* he_get_runtime_platform_version() {");
    builder.AppendLine("    return kRuntimePlatformVersion;");
    builder.AppendLine("}");
    builder.AppendLine();
    builder.AppendLine("bool he_get_runtime_dynamic_batching_enabled() {");
    builder.AppendLine("    return kRuntimeDynamicBatchingEnabled;");
    builder.AppendLine("}");
    return builder.ToString();
}
```

Also add the helper:

```csharp
static bool ReadBooleanOption(
    IReadOnlyDictionary<string, string> selectedGraphicsOptionValues,
    string optionId,
    bool defaultValue) {
    if (selectedGraphicsOptionValues == null) {
        throw new ArgumentNullException(nameof(selectedGraphicsOptionValues));
    } else if (string.IsNullOrWhiteSpace(optionId)) {
        throw new ArgumentException("Option id must be provided.", nameof(optionId));
    }

    if (!selectedGraphicsOptionValues.TryGetValue(optionId, out string value) || string.IsNullOrWhiteSpace(value)) {
        return defaultValue;
    }

    if (!bool.TryParse(value, out bool parsedValue)) {
        throw new InvalidOperationException($"Graphics option '{optionId}' must be a boolean value.");
    }

    return parsedValue;
}
```

- [ ] **Step 5: Pass the selected graphics options into the manifest writer**

In `builder/PspPlatformAssetBuilder.cs`, update the build call:

```csharp
RuntimeNativeManifestWriter.Write(
    request.GeneratedCoreCppRootPath,
    request.Manifest,
    request.SelectedGraphicsOptionValues);
```

- [ ] **Step 6: Run the focused manifest test to verify it passes**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter BuildAsync_rewrites_runtime_startup_manifest_with_dynamic_batching_setting -v minimal`

Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add builder/PspRuntimeNativeManifestWriter.cs builder/PspPlatformAssetBuilder.cs builder.tests/PspPlatformAssetBuilderTests.cs
git commit -m "Embed PSP dynamic batching runtime setting"
```

### Task 3: Read the Runtime Setting in PSP Boot and Wire It Into the 3D Renderer

**Files:**
- Modify: `src/platform/psp/PspBootHost.cpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`

- [ ] **Step 1: Add renderer-owned dynamic batching state and setter declarations**

In `src/platform/psp/rendering/PspRenderManager3D.hpp`, add these members:

```cpp
public:
    /// Enables or disables dynamic batching for compatible PSP fixed-function drawables.
    void SetDynamicBatchingEnabled(bool enabled);

private:
    /// Stores whether compatible PSP fixed-function drawables should batch dynamically.
    bool DynamicBatchingEnabled;
```

And initialize the flag in the constructor declaration path.

- [ ] **Step 2: Implement the setter and constructor initialization**

In `src/platform/psp/rendering/PspRenderManager3D.cpp`, update the constructor and add the setter:

```cpp
PspRenderManager3D::PspRenderManager3D()
    : CurrentView(float4x4::get_Identity()),
      CurrentProjection(float4x4::get_Identity()),
      CurrentCameraPosition(0.0f, 0.0f, 0.0f),
      RenderManager2D(nullptr),
      DynamicBatchingEnabled(true) {
}

void PspRenderManager3D::SetDynamicBatchingEnabled(bool enabled) {
    DynamicBatchingEnabled = enabled;
}
```

- [ ] **Step 3: Read the generated startup flag in the boot host**

In `src/platform/psp/PspBootHost.cpp`, inside `InitializeCore`, immediately after constructing `pspRenderManager3D`, add:

```cpp
const bool dynamicBatchingEnabled = he_get_runtime_dynamic_batching_enabled();
pspRenderManager3D->SetDynamicBatchingEnabled(dynamicBatchingEnabled);
PspBootTrace::WriteLine(std::string("Runtime dynamic batching enabled=") + (dynamicBatchingEnabled ? "true" : "false"));
```

- [ ] **Step 4: Verify the PSP runtime still builds from the managed side**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "BuildAsync_rewrites_runtime_startup_manifest_with_dynamic_batching_setting|Descriptor_and_definition_expose_dynamic_batching_graphics_setting" -v minimal`

Expected: PASS, confirming the new flag is declared and generated code is still valid from the builder side.

- [ ] **Step 5: Commit**

```bash
git add src/platform/psp/PspBootHost.cpp src/platform/psp/rendering/PspRenderManager3D.hpp src/platform/psp/rendering/PspRenderManager3D.cpp
git commit -m "Wire PSP dynamic batching runtime setting"
```

### Task 4: Add the PSP Dynamic Batching Data Structures and Fallback Routing

**Files:**
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`

- [ ] **Step 1: Add batch key and batch-entry structures**

In `src/platform/psp/rendering/PspRenderManager3D.hpp`, add declarations for the batching types:

```cpp
private:
    /// Identifies one compatible PSP fixed-function dynamic batch.
    struct DynamicBatchKey {
        const PspRuntimeModel* RuntimeModel;
        const PspRuntimeTexture* Texture;
        std::uint32_t BaseColorAbgr;
        std::uint8_t UsesLighting;
        std::uint8_t HasTexture;

        bool operator==(const DynamicBatchKey& other) const;
    };

    /// Stores one queued dynamic drawable that can be appended into a batch buffer.
    struct DynamicBatchEntry {
        IDrawable3D* Drawable;
        const PspRuntimeModel* RuntimeModel;
        PspRuntimeTexture* Texture;
        float4 BaseColor;
        bool UsesLighting;
        bool HasTexture;
    };
```

- [ ] **Step 2: Add renderer-private batch helpers**

In `src/platform/psp/rendering/PspRenderManager3D.hpp`, add these helper declarations:

```cpp
    /// Returns whether one PSP drawable can participate in the dynamic batching path.
    bool CanBatchDynamicDrawable(
        IDrawable3D* drawable,
        PspRuntimeModel* runtimeModel,
        PspRuntimeMaterial* runtimeMaterial,
        PspRuntimeTexture* texture,
        bool hasTexture,
        bool usesLighting) const;

    /// Submits all queued compatible drawables through the dynamic batching path.
    void FlushDynamicBatches(const std::vector<DynamicBatchEntry>& entries);
```

- [ ] **Step 3: Add a hash for the batch key and wire fallback routing**

At the top of `src/platform/psp/rendering/PspRenderManager3D.cpp`, add a hash struct in the anonymous namespace:

```cpp
struct DynamicBatchKeyHash {
    std::size_t operator()(const PspRenderManager3D::DynamicBatchKey& key) const {
        std::size_t hash = std::hash<const void*>()(key.RuntimeModel);
        hash ^= std::hash<const void*>()(key.Texture) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<std::uint32_t>()(key.BaseColorAbgr) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<std::uint8_t>()(key.UsesLighting) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<std::uint8_t>()(key.HasTexture) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};
```

Then in `Visit`, do not change the current single-draw path yet. Only add `CanBatchDynamicDrawable` and ensure the existing code path remains the fallback target for incompatible drawables and for `DynamicBatchingEnabled == false`.

- [ ] **Step 4: Run a focused native build to verify the structural change compiles**

Run: `rtk dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city\project.heproj --build psp --output C:\tmp\psp-dynamic-batching-compile-check`

Expected: PASS through PSP native compilation, even though batching behavior is not active yet.

- [ ] **Step 5: Commit**

```bash
git add src/platform/psp/rendering/PspRenderManager3D.hpp src/platform/psp/rendering/PspRenderManager3D.cpp
git commit -m "Prepare PSP dynamic batching renderer structures"
```

### Task 5: Queue Compatible Drawables Per Camera and Flush Batches

**Files:**
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`

- [ ] **Step 1: Add camera-pass batch storage on the renderer**

In `src/platform/psp/rendering/PspRenderManager3D.hpp`, add:

```cpp
    /// Stores compatible drawables collected during the current camera pass.
    std::vector<DynamicBatchEntry> PendingDynamicBatchEntries;
```

- [ ] **Step 2: Queue compatible drawables in `Visit`**

In `src/platform/psp/rendering/PspRenderManager3D.cpp`, inside `Visit`, after resolving `pspRuntimeModelData`, `pspRuntimeMaterial`, `baseColor`, `useLighting`, `texture`, and `hasTexture`, add this branch before the current draw submission:

```cpp
if (DynamicBatchingEnabled
    && CanBatchDynamicDrawable(
        drawable,
        pspRuntimeModelData,
        pspRuntimeMaterial,
        texture,
        hasTexture,
        useLighting)) {
    PendingDynamicBatchEntries.push_back(DynamicBatchEntry {
        drawable,
        pspRuntimeModelData,
        texture,
        baseColor,
        useLighting,
        hasTexture
    });
    return;
}
```

- [ ] **Step 3: Flush batches at the end of each camera pass**

In `RenderCamera`, reset and flush around the render queue visit:

```cpp
PendingDynamicBatchEntries.clear();
if (renderQueue != nullptr) {
    drawableCount = renderQueue->get_Count();
    ResolveSceneLighting();
    if (LightingSettings.Pipeline == PspLightingPipeline::FixedFunctionLambert) {
        ConfigureFixedFunctionSceneLighting(LightingSettings, CurrentLighting);
    } else {
        sceGuDisable(GU_LIGHT0);
        sceGuDisable(GU_LIGHTING);
    }
    renderQueue->VisitOrdered(this);
    FlushDynamicBatches(PendingDynamicBatchEntries);
}
PendingDynamicBatchEntries.clear();
```

- [ ] **Step 4: Implement grouping and fallback-safe batch flushing**

In `FlushDynamicBatches`, group entries by `DynamicBatchKey` with `std::unordered_map<DynamicBatchKey, std::vector<DynamicBatchEntry>, DynamicBatchKeyHash>`. Build the key using runtime model, texture pointer, packed base color, `UsesLighting`, and `HasTexture`. For now, leave the actual combined draw body as the next task; this task only needs the grouping structure and a safe no-op on empty input.

```cpp
void PspRenderManager3D::FlushDynamicBatches(const std::vector<DynamicBatchEntry>& entries) {
    if (entries.empty()) {
        return;
    }

    std::unordered_map<DynamicBatchKey, std::vector<DynamicBatchEntry>, DynamicBatchKeyHash> buckets;
    for (const DynamicBatchEntry& entry : entries) {
        DynamicBatchKey key {
            entry.RuntimeModel,
            entry.Texture,
            ConvertColorToAbgr(entry.BaseColor),
            static_cast<std::uint8_t>(entry.UsesLighting ? 1 : 0),
            static_cast<std::uint8_t>(entry.HasTexture ? 1 : 0)
        };

        buckets[key].push_back(entry);
    }

    for (const auto& pair : buckets) {
        const std::vector<DynamicBatchEntry>& batchEntries = pair.second;
        if (batchEntries.empty()) {
            continue;
        }
    }
}
```

- [ ] **Step 5: Run a fresh PSP export build to verify the queueing path compiles**

Run: `rtk dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city\project.heproj --build psp --output C:\tmp\psp-dynamic-batching-queue-check`

Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/platform/psp/rendering/PspRenderManager3D.hpp src/platform/psp/rendering/PspRenderManager3D.cpp
git commit -m "Queue PSP dynamic batch entries per camera"
```

### Task 6: Implement Combined Dynamic Draw Submission for Untextured and Textured Fixed-Function Drawables

**Files:**
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`

- [ ] **Step 1: Add world-space vertex emission helpers**

In the anonymous namespace of `src/platform/psp/rendering/PspRenderManager3D.cpp`, add helpers that transform model-space vertices into a combined batch buffer:

```cpp
float3 TransformPosition(const float3& position, const float4x4& worldMatrix) {
    return float3(
        (position.X * worldMatrix.M11) + (position.Y * worldMatrix.M21) + (position.Z * worldMatrix.M31) + worldMatrix.M41,
        (position.X * worldMatrix.M12) + (position.Y * worldMatrix.M22) + (position.Z * worldMatrix.M32) + worldMatrix.M42,
        (position.X * worldMatrix.M13) + (position.Y * worldMatrix.M23) + (position.Z * worldMatrix.M33) + worldMatrix.M43);
}
```

And emit GU-ready world-space vertices for both layouts using `GU_TRANSFORM_3D`.

- [ ] **Step 2: Implement combined untextured batch submission**

Add a helper that builds one transient `PspLitVertex` stream across all entries in a bucket:

```cpp
void SubmitDynamicUntexturedBatch(
    const std::vector<PspRenderManager3D::DynamicBatchEntry>& batchEntries,
    const PspLightingSettings& lightingSettings,
    const PspSceneLightingSnapshot& lightingSnapshot) {
    std::size_t totalVertexCount = 0;
    for (const PspRenderManager3D::DynamicBatchEntry& entry : batchEntries) {
        totalVertexCount += static_cast<std::size_t>(entry.RuntimeModel->GetFixedFunctionVertexCount());
    }

    if (totalVertexCount < 3) {
        return;
    }

    PspLitVertex* vertices = static_cast<PspLitVertex*>(sceGuGetMemory(sizeof(PspLitVertex) * totalVertexCount));
    std::size_t vertexOffset = 0;
    for (const PspRenderManager3D::DynamicBatchEntry& entry : batchEntries) {
        float4x4 worldMatrix = BuildWorldMatrix(entry.Drawable->get_Parent());
        const PspRuntimeModel::FixedFunctionVertex* sourceVertices = entry.RuntimeModel->GetFixedFunctionVertices();
        int32_t vertexCount = entry.RuntimeModel->GetFixedFunctionVertexCount();
        for (int32_t index = 0; index < vertexCount; index++) {
            const PspRuntimeModel::FixedFunctionVertex& sourceVertex = sourceVertices[index];
            const float3 position(sourceVertex.X, sourceVertex.Y, sourceVertex.Z);
            const float3 sourceNormal(sourceVertex.NX, sourceVertex.NY, sourceVertex.NZ);
            const float3 worldPosition = TransformPosition(position, worldMatrix);
            const float3 worldNormal = float3::Normalize(RotateNormal(sourceNormal, entry.Drawable->get_Parent()));
            const float4 litColor = EvaluateCpuLitColor(entry.BaseColor, worldNormal, entry.UsesLighting, lightingSettings, lightingSnapshot);

            vertices[vertexOffset++] = PspLitVertex {
                ConvertColorToAbgr(litColor),
                worldPosition.X,
                worldPosition.Y,
                worldPosition.Z
            };
        }
    }

    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_LIGHT0);
    sceGuDisable(GU_LIGHTING);
    sceGumDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        static_cast<int>(totalVertexCount),
        nullptr,
        vertices);
}
```

- [ ] **Step 3: Implement combined textured batch submission**

Add the textured equivalent using `PspTexturedLitVertex`, preserving each entry’s UVs and color while binding the shared texture once:

```cpp
void SubmitDynamicTexturedBatch(
    const std::vector<PspRenderManager3D::DynamicBatchEntry>& batchEntries,
    PspRuntimeTexture* texture,
    const PspLightingSettings& lightingSettings,
    const PspSceneLightingSnapshot& lightingSnapshot) {
    if (texture == nullptr || !texture->HasPixels()) {
        throw std::runtime_error("PSP dynamic textured batching requires a valid runtime texture.");
    }

    std::size_t totalVertexCount = 0;
    for (const PspRenderManager3D::DynamicBatchEntry& entry : batchEntries) {
        totalVertexCount += static_cast<std::size_t>(entry.RuntimeModel->GetFixedFunctionTexturedVertexCount());
    }

    if (totalVertexCount < 3) {
        return;
    }

    PspTexturedLitVertex* vertices = static_cast<PspTexturedLitVertex*>(sceGuGetMemory(sizeof(PspTexturedLitVertex) * totalVertexCount));
    std::size_t vertexOffset = 0;
    for (const PspRenderManager3D::DynamicBatchEntry& entry : batchEntries) {
        float4x4 worldMatrix = BuildWorldMatrix(entry.Drawable->get_Parent());
        const PspRuntimeModel::FixedFunctionTexturedVertex* sourceVertices = entry.RuntimeModel->GetFixedFunctionTexturedVertices();
        int32_t vertexCount = entry.RuntimeModel->GetFixedFunctionTexturedVertexCount();
        for (int32_t index = 0; index < vertexCount; index++) {
            const PspRuntimeModel::FixedFunctionTexturedVertex& sourceVertex = sourceVertices[index];
            const float3 position(sourceVertex.X, sourceVertex.Y, sourceVertex.Z);
            const float3 sourceNormal(sourceVertex.NX, sourceVertex.NY, sourceVertex.NZ);
            const float3 worldPosition = TransformPosition(position, worldMatrix);
            const float3 worldNormal = float3::Normalize(RotateNormal(sourceNormal, entry.Drawable->get_Parent()));
            const float4 litColor = EvaluateCpuLitColor(entry.BaseColor, worldNormal, entry.UsesLighting, lightingSettings, lightingSnapshot);

            vertices[vertexOffset++] = PspTexturedLitVertex {
                sourceVertex.U,
                sourceVertex.V,
                ConvertColorToAbgr(litColor),
                worldPosition.X,
                worldPosition.Y,
                worldPosition.Z
            };
        }
    }

    BindTexture(texture);
    sceGuDisable(GU_LIGHT0);
    sceGuDisable(GU_LIGHTING);
    sceGumDrawArray(
        GU_TRIANGLES,
        GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        static_cast<int>(totalVertexCount),
        nullptr,
        vertices);
}
```

- [ ] **Step 4: Call the combined submission helpers from `FlushDynamicBatches`**

Replace the empty loop body in `FlushDynamicBatches` with:

```cpp
for (const auto& pair : buckets) {
    const DynamicBatchKey& key = pair.first;
    const std::vector<DynamicBatchEntry>& batchEntries = pair.second;
    if (batchEntries.empty()) {
        continue;
    }

    if (key.HasTexture != 0) {
        SubmitDynamicTexturedBatch(batchEntries, batchEntries[0].Texture, LightingSettings, CurrentLighting);
    } else {
        SubmitDynamicUntexturedBatch(batchEntries, LightingSettings, CurrentLighting);
    }
}
```

- [ ] **Step 5: Build the PSP export and verify native compilation still passes**

Run: `rtk dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city\project.heproj --build psp --output C:\tmp\psp-dynamic-batching-build`

Expected: PASS with `EBOOT.PBP` produced.

- [ ] **Step 6: Commit**

```bash
git add src/platform/psp/rendering/PspRenderManager3D.cpp
git commit -m "Add PSP dynamic fixed-function batch submission"
```

### Task 7: Verify Runtime Behavior, Fallback, and Performance

**Files:**
- Modify: `builder.tests/CityColoredCubeGridSceneTests.cs` (only if a focused build-setting regression is needed)
- Modify: `tools/update_psp_demo_disc_build.ps1` (only if local convenience scripting needs the new option default)

- [ ] **Step 1: Run the PSP builder test slice**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "PspPlatformAssetBuilderTests|CityColoredCubeGridSceneTests" -v minimal`

Expected: PASS

- [ ] **Step 2: Build a PSP export with dynamic batching enabled**

Run: `rtk dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city\project.heproj --build psp --output C:\tmp\psp-dynamic-batching-enabled`

Expected: PASS

- [ ] **Step 3: Stage the export to PPSSPP and capture the boot log**

Run: `rtk powershell.exe -File C:\dev\helworks\helengine-psp\tools\run_ppsspp_boot_check.ps1`

Expected: PASS with `LoadStartupScene id=colored_cube_grid` when the build config is temporarily pointed there, and a boot log at `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`

- [ ] **Step 4: Verify the enabled-path profiler shape**

Inspect the boot log and confirm:

```text
PspPerfFrame3D ...
```

Expected:
- pure 3D time lower than the previous `~2.37 ms`
- fewer effective 3D submissions in the profiler traces
- unchanged visual scene behavior

- [ ] **Step 5: Build again with `dynamic-batching-enabled=false` to verify fallback**

Set the PSP graphics option to `false` in the active build config and run:

`rtk dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city\project.heproj --build psp --output C:\tmp\psp-dynamic-batching-disabled`

Expected: PASS

- [ ] **Step 6: Stage the disabled build and capture the boot log**

Run: `rtk powershell.exe -File C:\dev\helworks\helengine-psp\tools\run_ppsspp_boot_check.ps1`

Expected: PASS with profiler numbers returning to the previous non-batched range.

- [ ] **Step 7: Commit**

```bash
git add builder.tests/CityColoredCubeGridSceneTests.cs tools/update_psp_demo_disc_build.ps1
git commit -m "Verify PSP dynamic batching runtime behavior"
```

## Self-Review

- Spec coverage:
  - PSP graphics setting on `psp-forward`: Task 1
  - build/runtime plumbing of the setting: Tasks 2 and 3
  - renderer-side dynamic batching for compatible fixed-function drawables: Tasks 4, 5, and 6
  - fallback behavior when disabled or incompatible: Tasks 4, 5, and 7
  - PPSSPP verification on `colored_cube_grid`: Task 7
- Placeholder scan:
  - No `TODO`, `TBD`, or “similar to above” shortcuts remain.
  - Each task includes explicit files, commands, and code snippets.
- Type consistency:
  - The setting id is consistently `dynamic-batching-enabled`.
  - The generated manifest accessor is consistently `he_get_runtime_dynamic_batching_enabled()`.
  - The renderer switch is consistently `DynamicBatchingEnabled`.
