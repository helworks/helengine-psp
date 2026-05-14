# Helengine PSP City Cube Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a real PSP platform target that the editor can build end-to-end for the `city` project, producing a PSP homebrew app folder with `EBOOT.PBP` and enough runtime support to load and render the authored `cube_test` scene as a rotating unlit cube in PPSSPP.

**Architecture:** Keep the existing Helengine editor-owned build graph intact. The editor must regenerate generated-core C++ for platform `psp`, cook the selected scene and assets, write runtime native manifests, and then invoke a dedicated PSP builder assembly. The PSP builder stages a PSP homebrew app layout and invokes the native PSP player build, while the native PSP host compiles the generated core and implements only the minimal boot, file access, input, and 3D rendering path required by `cube_test.helen`.

**Tech Stack:** .NET 9, xUnit, Helengine baseplatform/editor build graph, PSPDEV/PSPSDK, CMake, GNU Make, C++17, PSP GU/GUM, PPSSPP

---

## File Structure

- Create: `builder/helengine.psp.builder.csproj`
  Purpose: PSP builder project that exposes platform metadata and the build entrypoint consumed by the editor.
- Create: `builder/Program.cs`
  Purpose: small CLI for `--describe` and `--smoke-test` parity with the PS2 builder.
- Create: `builder/PspPlatformDefinitionFactory.cs`
  Purpose: typed PSP platform metadata, including build/graphics/codegen/storage/media profiles.
- Create: `builder/PspPlatformAssetBuilder.cs`
  Purpose: main PSP `IPlatformAssetBuilder` implementation.
- Create: `builder/PspBuildWorkspace.cs`
  Purpose: typed workspace model for PSP staging paths, generated core root, and final app folder layout.
- Create: `builder/IPspNativeBuildExecutor.cs`
  Purpose: abstraction for native PSP build execution so the builder remains testable.
- Create: `builder/PspNativeBuildExecutor.cs`
  Purpose: Docker/Make-based native PSP build executor.
- Create: `builder/PspAppLayoutWriter.cs`
  Purpose: copies cooked assets into the PSP homebrew app tree and validates required output paths.
- Create: `builder.tests/helengine.psp.builder.tests.csproj`
  Purpose: PSP builder test project mirroring the Windows/PS2 test shape.
- Create: `builder.tests/PspPlatformAssetBuilderTests.cs`
  Purpose: builder metadata, staging, generated-core, and final app-layout tests.
- Create: `builder.tests/PspNativeBuildExecutorTests.cs`
  Purpose: command construction tests for the native build executor.
- Create: `builder.tests/RecordingDiagnosticReporter.cs`
  Purpose: test helper for collecting diagnostics.
- Create: `builder.tests/RecordingProgressReporter.cs`
  Purpose: test helper for collecting progress updates.
- Modify: `/mnt/c/dev/helworks/helengine/user_settings/platforms.json`
  Purpose: register the PSP builder/player installation with the editor.
- Modify: `/mnt/c/dev/helworks/helengine/engine/helengine.editor/managers/project/EditorGeneratedCoreRegenerationService.cs`
  Purpose: add PSP-specific preprocessor symbols to editor-owned generated-core regeneration.
- Modify: `/mnt/c/dev/helworks/helengine/engine/helengine.editor.tests/managers/project/EditorGeneratedCoreRegenerationServiceTests.cs`
  Purpose: lock down PSP generated-core regeneration expectations.
- Modify: `/mnt/c/dev/helprojs/city/project.heproj`
  Purpose: advertise `psp` as a supported project platform.
- Modify: `/mnt/c/dev/helprojs/city/settings/platforms.json`
  Purpose: advertise PSP as a shared project platform profile.
- Create: `/mnt/c/dev/helprojs/city/settings/platform.psp.json`
  Purpose: seed shared PSP build/graphics/codegen defaults for the city project.
- Modify: `CMakeLists.txt`
  Purpose: compile the generated core, runtime manifests, and new PSP platform files into the PSP player.
- Modify: `Makefile`
  Purpose: require `HELENGINE_CORE_CPP_ROOT` for real builds and keep the PSP native build contract explicit.
- Modify: `README.md`
  Purpose: document the editor-driven PSP build flow and staged app output shape.
- Modify: `src/main.cpp`
  Purpose: keep the PSP entrypoint stable while the host expands.
- Modify: `src/platform/psp/PspBootHost.hpp`
  Purpose: declare the full PSP boot, generated-core, and runtime-loading lifecycle.
- Modify: `src/platform/psp/PspBootHost.cpp`
  Purpose: initialize PSP subsystems, generated core, runtime scene catalog, and startup scene loading.
- Create: `src/platform/psp/PspAppRootPathResolver.hpp`
  Purpose: resolve the PSP homebrew app directory at runtime.
- Create: `src/platform/psp/PspAppRootPathResolver.cpp`
  Purpose: implement PSP app-root discovery from the running module path.
- Create: `src/platform/psp/PspInputBackend.hpp`
  Purpose: minimal PSP input backend required by generated core initialization.
- Create: `src/platform/psp/PspInputBackend.cpp`
  Purpose: implement the initial no-frills PSP input backend.
- Create: `src/platform/psp/PspPackagedAssetLoader.hpp`
  Purpose: load cooked packaged assets by cooked-relative path from the PSP app root.
- Create: `src/platform/psp/PspPackagedAssetLoader.cpp`
  Purpose: implement PSP packaged asset loading through generated-core file abstractions.
- Create: `src/platform/psp/PspRuntimeSceneCatalogFactory.hpp`
  Purpose: build a generated-core `RuntimeSceneCatalog` from the generated native scene manifest.
- Create: `src/platform/psp/PspRuntimeSceneCatalogFactory.cpp`
  Purpose: implement the scene catalog bridge for generated runtime metadata.
- Create: `src/platform/psp/rendering/PspRenderManager2D.hpp`
  Purpose: no-op 2D render manager required by generated core initialization.
- Create: `src/platform/psp/rendering/PspRenderManager2D.cpp`
  Purpose: implement a minimal 2D stub so the 3D-only bring-up can initialize core.
- Create: `src/platform/psp/rendering/PspRenderManager3D.hpp`
  Purpose: minimal PSP render manager that accepts generated-core 3D drawables.
- Create: `src/platform/psp/rendering/PspRenderManager3D.cpp`
  Purpose: transform and submit the authored cube mesh as an unlit GU triangle list.

### Task 1: Create The PSP Builder Scaffold And Metadata

**Files:**
- Create: `builder/helengine.psp.builder.csproj`
- Create: `builder/Program.cs`
- Create: `builder/PspPlatformDefinitionFactory.cs`
- Create: `builder/PspPlatformAssetBuilder.cs`
- Create: `builder.tests/helengine.psp.builder.tests.csproj`
- Create: `builder.tests/PspPlatformAssetBuilderTests.cs`
- Create: `builder.tests/RecordingDiagnosticReporter.cs`
- Create: `builder.tests/RecordingProgressReporter.cs`

- [ ] **Step 1: Write the failing PSP builder metadata test**

```csharp
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;
using Xunit;

namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the PSP builder metadata surface published to the editor.
/// </summary>
public sealed class PspPlatformAssetBuilderTests {
    /// <summary>
    /// Ensures the PSP builder exposes the required build, graphics, codegen, storage, and media metadata.
    /// </summary>
    [Fact]
    public void Descriptor_and_definition_expose_psp_metadata() {
        PspPlatformAssetBuilder builder = new();

        Assert.Equal("helengine.psp.builder", builder.Descriptor.BuilderId);
        Assert.Equal("psp", builder.Descriptor.TargetPlatformId);
        Assert.Equal("psp", builder.Definition.PlatformId);
        Assert.Contains(builder.Definition.BuildProfiles, profile => profile.ProfileId == "debug");
        Assert.Contains(builder.Definition.GraphicsProfiles, profile => profile.ProfileId == "psp-forward");
        Assert.Contains(builder.Definition.CodegenProfiles, profile =>
            profile.ProfileId == "default"
            && profile.OutputLanguage == PlatformCodegenLanguage.Cpp
            && profile.Endianness == PlatformSerializationEndianness.LittleEndian);
        Assert.Contains(builder.Definition.StorageProfiles, profile =>
            profile.ProfileId == "homebrew-app"
            && profile.RuntimeSpecializationId == "psp-homebrew-app");
        Assert.Contains(builder.Definition.MediaProfiles, profile => profile.ProfileId == "psp-game-folder");
        Assert.Contains(builder.Definition.ComponentCompatibilities, compatibility =>
            compatibility.ComponentTypeId == "helengine.meshcomponent");
        Assert.Contains(builder.Definition.ComponentCompatibilities, compatibility =>
            compatibility.ComponentTypeId == "helengine.cameracomponent");
    }
}
```

- [ ] **Step 2: Run the test to verify the PSP builder does not exist yet**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~Descriptor_and_definition_expose_psp_metadata" -v minimal`

Expected: FAIL with missing project or missing PSP builder types.

- [ ] **Step 3: Create the PSP builder projects and metadata surface**

```xml
<!-- builder/helengine.psp.builder.csproj -->
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net9.0</TargetFramework>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>disable</Nullable>
    <OutputType>Exe</OutputType>
    <HelengineRoot Condition="'$(HelengineRoot)' == '' and '$(HELENGINE_ROOT)' != ''">$(HELENGINE_ROOT)</HelengineRoot>
    <HelengineRoot Condition="'$(HelengineRoot)' == ''">$(MSBuildThisFileDirectory)..\..\helengine</HelengineRoot>
  </PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="$(HelengineRoot)\engine\helengine.baseplatform\helengine.baseplatform.csproj" />
    <ProjectReference Include="$(HelengineRoot)\engine\helengine.files\helengine.files.csproj" />
  </ItemGroup>
</Project>
```

```csharp
// builder/PspPlatformDefinitionFactory.cs
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;

namespace helengine.psp.builder;

/// <summary>
/// Creates the typed PSP builder metadata consumed by the editor.
/// </summary>
public static class PspPlatformDefinitionFactory {
    /// <summary>
    /// Creates the PSP platform definition for the first city cube-test milestone.
    /// </summary>
    /// <returns>The PSP platform definition.</returns>
    public static PlatformDefinition Create() {
        return new PlatformDefinition(
            "psp",
            "PlayStation Portable",
            [
                new PlatformBuildProfileDefinition(
                    "debug",
                    "Debug",
                    "Debug PSP homebrew build",
                    "psp-forward",
                    "default",
                    [
                        new PlatformSettingDefinition("texture-scale-percent", "Texture Scale Percent", PlatformSettingKind.Text, "100", true, []),
                        new PlatformSettingDefinition("shader-variant-pruning", "Shader Variant Pruning", PlatformSettingKind.Boolean, "true", true, [])
                    ])
            ],
            [
                new PlatformGraphicsProfileDefinition(
                    "psp-forward",
                    "PSP Forward",
                    "Minimal PSP forward renderer for the authored cube-test scene.",
                    [
                        new PlatformSettingDefinition("default-width", "Default Width", PlatformSettingKind.Text, "480", true, []),
                        new PlatformSettingDefinition("default-height", "Default Height", PlatformSettingKind.Text, "272", true, []),
                        new PlatformSettingDefinition("vsync-enabled", "VSync Enabled", PlatformSettingKind.Boolean, "true", true, []),
                        new PlatformSettingDefinition("fullscreen-enabled", "Fullscreen Enabled", PlatformSettingKind.Boolean, "true", true, [])
                    ])
            ],
            [
                new PlatformAssetRequirementDefinition("scene", "Scene", true, ["helen"]),
                new PlatformAssetRequirementDefinition("texture", "Texture", false, ["png", "tga", "jpg"])
            ],
            [
                new PlatformMaterialSchemaDefinition(
                    "psp-standard-unlit",
                    "PSP Standard Unlit",
                    ["psp-forward"],
                    [
                        new PlatformMaterialFieldDefinition("base-color", "Base Color", PlatformMaterialFieldKind.Color, "#ffffff", false, []),
                        new PlatformMaterialFieldDefinition("texture-id", "Diffuse Texture", PlatformMaterialFieldKind.AssetReference, string.Empty, false, [])
                    ])
            ],
            [
                new PlatformComponentCompatibilityDefinition("helengine.meshcomponent", PlatformComponentCompatibilityKind.Transform, "Mesh components are normalized during packaging.", string.Empty),
                new PlatformComponentCompatibilityDefinition("helengine.cameracomponent", PlatformComponentCompatibilityKind.Transform, "Camera components are normalized during packaging.", string.Empty),
                new PlatformComponentCompatibilityDefinition("helengine.directionallightcomponent", PlatformComponentCompatibilityKind.PassThrough, "Directional light payloads can deserialize even when the first PSP renderer ignores them.", string.Empty)
            ],
            [
                new PlatformCodegenProfileDefinition(
                    "default",
                    "Default",
                    "PSP C# to C++ codegen profile",
                    PlatformCodegenLanguage.Cpp,
                    PlatformSerializationEndianness.LittleEndian,
                    [
                        new PlatformSettingDefinition("write-conversion-report", "Write Conversion Report", PlatformSettingKind.Boolean, "true", true, []),
                        new PlatformSettingDefinition("include-project-defined-preprocessor-symbols", "Include Project Symbols", PlatformSettingKind.Boolean, "false", true, []),
                        new PlatformSettingDefinition("load-native-runtime-metadata", "Load Native Runtime Metadata", PlatformSettingKind.Boolean, "true", true, [])
                    ])
            ],
            [
                new PlatformStorageProfileDefinition("homebrew-app", "Homebrew App", PlatformStorageProfileKind.LooseFiles, "psp-homebrew-app", allowContainerSegmentation: false)
            ],
            [
                new PlatformMediaProfileDefinition("psp-game-folder", "PSP Game Folder", PlatformMediaLayoutKind.InstallTree, allowPhysicalDuplication: false, preferLocalityOverDeduplication: true)
            ]);
    }
}
```

```csharp
// builder/PspPlatformAssetBuilder.cs
using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.psp.builder;

/// <summary>
/// Implements the PSP platform asset builder contract.
/// </summary>
public sealed class PspPlatformAssetBuilder : IPlatformAssetBuilder {
    /// <summary>
    /// Initializes the PSP builder with its typed metadata.
    /// </summary>
    public PspPlatformAssetBuilder() {
        Descriptor = new PlatformBuilderDescriptor(
            "helengine.psp.builder",
            "1.0.0",
            "psp",
            new EngineCompatibilityRange("1.0.0", "999.0.0"),
            new ManifestCompatibilityRange(1, 3),
            ["psp"],
            ["debug"]);
        Definition = PspPlatformDefinitionFactory.Create();
    }

    /// <summary>
    /// Gets the explicit builder descriptor for the PSP builder assembly.
    /// </summary>
    public PlatformBuilderDescriptor Descriptor { get; }

    /// <summary>
    /// Gets the typed PSP platform definition exposed to the editor.
    /// </summary>
    public PlatformDefinition Definition { get; }

    /// <summary>
    /// Rejects material cook requests until the PSP runtime material payload is implemented.
    /// </summary>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        throw new NotSupportedException("PSP material cooking is not available in the metadata scaffold.");
    }

    /// <summary>
    /// Rejects build execution until the PSP staging and native executor flow is implemented.
    /// </summary>
    public Task<PlatformBuildReport> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        throw new NotSupportedException("PSP build execution is not available in the metadata scaffold.");
    }
}
```

- [ ] **Step 4: Add the PSP builder smoke-test entrypoint**

```csharp
// builder/Program.cs
namespace helengine.psp.builder;

public static class Program {
    public static int Main(string[] args) {
        if (args.Length > 0 && string.Equals(args[0], "--describe", StringComparison.OrdinalIgnoreCase)) {
            PspPlatformAssetBuilder builder = new();
            Console.WriteLine(builder.Descriptor.BuilderId);
            Console.WriteLine(builder.Descriptor.TargetPlatformId);
            Console.WriteLine(builder.Definition.DisplayName);
            Console.WriteLine(builder.Definition.BuildProfiles.Length);
            Console.WriteLine(builder.Definition.GraphicsProfiles.Length);
            return 0;
        }

        Console.WriteLine("helengine.psp.builder --describe");
        return 0;
    }
}
```

- [ ] **Step 5: Run the PSP builder metadata tests**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~PspPlatformAssetBuilderTests" -v minimal`

Expected: PASS with the metadata test green and no skipped assertions.

- [ ] **Step 6: Commit the PSP builder scaffold**

```bash
rtk git add builder builder.tests
rtk git commit -m "Add PSP builder metadata scaffold"
```

### Task 2: Register PSP With The Editor And The City Project

**Files:**
- Modify: `/mnt/c/dev/helworks/helengine/engine/helengine.editor/managers/project/EditorGeneratedCoreRegenerationService.cs`
- Modify: `/mnt/c/dev/helworks/helengine/engine/helengine.editor.tests/managers/project/EditorGeneratedCoreRegenerationServiceTests.cs`
- Modify: `/mnt/c/dev/helworks/helengine/user_settings/platforms.json`
- Modify: `/mnt/c/dev/helprojs/city/project.heproj`
- Modify: `/mnt/c/dev/helprojs/city/settings/platforms.json`
- Create: `/mnt/c/dev/helprojs/city/settings/platform.psp.json`

- [ ] **Step 1: Write the failing PSP generated-core regeneration test**

```csharp
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;
using Xunit;

namespace helengine.editor.tests.managers.project;

public sealed partial class EditorGeneratedCoreRegenerationServiceTests {
    /// <summary>
    /// Verifies PSP generated-core regeneration forwards PSP-specific portable-input symbols.
    /// </summary>
    [Fact]
    public void ResolvePortableInputPreprocessorSymbols_whenPlatformIsPsp_returns_psp_symbols() {
        PlatformDefinition definition = new PlatformDefinition(
            "psp",
            "PlayStation Portable",
            Array.Empty<PlatformBuildProfileDefinition>(),
            Array.Empty<PlatformGraphicsProfileDefinition>(),
            Array.Empty<PlatformAssetRequirementDefinition>(),
            Array.Empty<PlatformMaterialSchemaDefinition>(),
            Array.Empty<PlatformComponentCompatibilityDefinition>(),
            Array.Empty<PlatformCodegenProfileDefinition>(),
            Array.Empty<PlatformStorageProfileDefinition>(),
            Array.Empty<PlatformMediaProfileDefinition>());

        IReadOnlyList<string> symbols = EditorGeneratedCoreRegenerationService.ResolvePortableInputPreprocessorSymbols(definition);

        Assert.Contains("PSP_PLATFORM", symbols);
        Assert.Contains("HELENGINE_CODEGEN_DISABLE_MENU_REFLECTION", symbols);
        Assert.Contains("HELENGINE_CODEGEN_DISABLE_RUNTIME_SCRIPT_REFLECTION", symbols);
    }
}
```

- [ ] **Step 2: Run the editor test to verify PSP symbols are not forwarded yet**

Run: `rtk dotnet test /mnt/c/dev/helworks/helengine/engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~ResolvePortableInputPreprocessorSymbols_whenPlatformIsPsp_returns_psp_symbols" -v minimal`

Expected: FAIL because `PSP_PLATFORM` is missing from the generated symbol list.

- [ ] **Step 3: Add PSP preprocessor symbol handling to the editor regeneration service**

```csharp
// /mnt/c/dev/helworks/helengine/engine/helengine.editor/managers/project/EditorGeneratedCoreRegenerationService.cs
if (string.Equals(platformDefinition.PlatformId, "psp", StringComparison.OrdinalIgnoreCase)) {
    return [
        "PSP_PLATFORM",
        "HELENGINE_CODEGEN_DISABLE_MENU_REFLECTION",
        "HELENGINE_CODEGEN_DISABLE_RUNTIME_SCRIPT_REFLECTION"
    ];
}
```

- [ ] **Step 4: Register PSP in the editor installation and city project settings**

```json
// /mnt/c/dev/helworks/helengine/user_settings/platforms.json
{
  "engineVersion": "1.0.0+13db86b8a91031015e3d0475799b6e6b1a56b309",
  "platformId": "psp",
  "displayName": "PSP",
  "builderAssemblyPath": "../../helengine-psp/builder/bin/Debug/net9.0/helengine.psp.builder.dll",
  "playerSourceRootPath": "../../helengine-psp",
  "generatedCoreCppRootPath": "../tmp/helengine-core-cpp-regenerated",
  "codegenToolPath": "../../csharpcodegen/codegen/bin/Debug/net9.0/codegen.exe"
}
```

```json
// /mnt/c/dev/helprojs/city/project.heproj
"supportedPlatforms": [
  "windows",
  "ps2",
  "psp"
]
```

```json
// /mnt/c/dev/helprojs/city/settings/platforms.json
{
  "supportedPlatforms": [
    "ps2",
    "psp",
    "windows"
  ]
}
```

```json
// /mnt/c/dev/helprojs/city/settings/platform.psp.json
{
  "platformId": "psp",
  "build": {
    "selectedBuildProfileId": "",
    "textureScalePercent": 100,
    "shaderVariantPruningEnabled": true,
    "selectedOptionValues": {}
  },
  "graphics": {
    "selectedGraphicsProfileId": "",
    "defaultWidth": 480,
    "defaultHeight": 272,
    "vSyncEnabled": true,
    "fullscreenEnabled": true,
    "rendererDepthPrepassMode": 0,
    "rendererShadowQualityTier": "disabled",
    "rendererHdrEnabled": false,
    "rendererPostProcessTier": 0,
    "selectedOptionValues": {}
  },
  "codegen": {
    "selectedCodegenProfileId": "",
    "selectedOptionValues": {}
  }
}
```

- [ ] **Step 5: Re-run the editor generated-core test**

Run: `rtk dotnet test /mnt/c/dev/helworks/helengine/engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~ResolvePortableInputPreprocessorSymbols_whenPlatformIsPsp_returns_psp_symbols" -v minimal`

Expected: PASS with the PSP symbol assertion green.

- [ ] **Step 6: Commit the editor and project registration changes**

```bash
rtk git -C /mnt/c/dev/helworks/helengine add engine/helengine.editor/managers/project/EditorGeneratedCoreRegenerationService.cs engine/helengine.editor.tests/managers/project/EditorGeneratedCoreRegenerationServiceTests.cs user_settings/platforms.json
rtk git -C /mnt/c/dev/helworks/helengine commit -m "Register PSP platform metadata"
rtk git add /mnt/c/dev/helprojs/city/project.heproj /mnt/c/dev/helprojs/city/settings/platforms.json /mnt/c/dev/helprojs/city/settings/platform.psp.json
rtk git -C /mnt/c/dev/helprojs/city commit -m "Add PSP project settings"
```

### Task 3: Implement PSP Builder Execution And App-Folder Staging

**Files:**
- Create: `builder/PspBuildWorkspace.cs`
- Create: `builder/IPspNativeBuildExecutor.cs`
- Create: `builder/PspNativeBuildExecutor.cs`
- Create: `builder/PspAppLayoutWriter.cs`
- Modify: `builder/PspPlatformAssetBuilder.cs`
- Modify: `builder/Program.cs`
- Modify: `builder.tests/PspPlatformAssetBuilderTests.cs`
- Create: `builder.tests/PspNativeBuildExecutorTests.cs`

- [ ] **Step 1: Write the failing PSP build staging test**

```csharp
[Fact]
public async Task BuildAsync_whenGivenGeneratedCoreAndCookedArtifacts_produces_psp_game_folder() {
    string workingRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
    string outputRoot = Path.Combine(workingRoot, "out");
    string sourceRoot = Path.Combine(workingRoot, "staging");
    string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
    string sceneSourcePath = Path.Combine(sourceRoot, "cooked", "scenes", "rendering", "cube_test.hasset");
    string modelSourcePath = Path.Combine(sourceRoot, "cooked", "models", "cube.hasset");
    string materialSourcePath = Path.Combine(sourceRoot, "cooked", "materials", "standard.hasset");

    Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath)!);
    Directory.CreateDirectory(Path.GetDirectoryName(modelSourcePath)!);
    Directory.CreateDirectory(Path.GetDirectoryName(materialSourcePath)!);
    Directory.CreateDirectory(generatedCoreRoot);
    File.WriteAllText(sceneSourcePath, "scene payload");
    File.WriteAllText(modelSourcePath, "model payload");
    File.WriteAllText(materialSourcePath, "material payload");
    File.WriteAllText(Path.Combine(generatedCoreRoot, "helengine_core_amalgamated.cpp"), "// generated");

    string previousDirectory = Directory.GetCurrentDirectory();
    try {
        Directory.SetCurrentDirectory(sourceRoot);

        PlatformBuildManifest manifest = new(
            3,
            "city",
            "1.0.0",
            "1.0.0",
            "scenes/rendering/cube_test.helen",
            [
                new PlatformBuildScene(
                    "scenes/rendering/cube_test.helen",
                    "Cube Test",
                    "cooked/scenes/rendering/cube_test.hasset",
                    [],
                    [new KeyValuePair<string, string>("cooked-relative-path", "cooked/scenes/rendering/cube_test.hasset")])
            ],
            Array.Empty<PlatformBuildAsset>(),
            [
                new PlatformBuildArtifact("cooked/scenes/rendering/cube_test.hasset", "scene:cube-test", "sha256:scene", "scene", "shared"),
                new PlatformBuildArtifact("cooked/models/cube.hasset", "model:cube", "sha256:model", "model", "shared"),
                new PlatformBuildArtifact("cooked/materials/standard.hasset", "material:standard", "sha256:material", "material", "shared")
            ],
            Array.Empty<PlatformBuildCodeModule>(),
            Array.Empty<PlatformArtifactPlacement>(),
            new PlatformContainerWritePlan("psp-homebrew", Array.Empty<PlatformContainerArtifact>()));

        PlatformBuildRequest request = new(
            manifest,
            [new PlatformBuildTargetVariant("psp-debug", "psp", "psp", "debug")],
            [new PlatformCookProfile("default", "Default", new PlatformCookProfileCapabilities("psp", "raw", "rgba", "psp-scene-v1", PlatformSerializationEndianness.LittleEndian))],
            outputRoot,
            Path.Combine(workingRoot, "tmp"),
            selectedBuildProfileId: "debug",
            selectedGraphicsProfileId: "psp-forward",
            selectedCodegenProfileId: "default",
            selectedBuildOptionValues: new Dictionary<string, string>(),
            selectedGraphicsOptionValues: new Dictionary<string, string>(),
            selectedCodegenOptionValues: new Dictionary<string, string>(),
            generatedCoreCppRootPath: generatedCoreRoot,
            selectedMediaProfileId: "psp-game-folder",
            selectedStorageProfileId: "homebrew-app");

        RecordingPspNativeBuildExecutor nativeBuildExecutor = new();
        PspPlatformAssetBuilder builder = new(nativeBuildExecutor);

        PlatformBuildReport report = await builder.BuildAsync(
            request,
            new RecordingProgressReporter(),
            new RecordingDiagnosticReporter(),
            CancellationToken.None);

        Assert.True(report.Succeeded);
        Assert.True(File.Exists(Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "EBOOT.PBP")));
        Assert.True(File.Exists(Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "cooked", "scenes", "rendering", "cube_test.hasset")));
        Assert.True(File.Exists(Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "cooked", "models", "cube.hasset")));
        Assert.True(File.Exists(Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "cooked", "materials", "standard.hasset")));
    } finally {
        Directory.SetCurrentDirectory(previousDirectory);
    }
}
```

- [ ] **Step 2: Run the PSP builder tests to verify build execution is still missing**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~BuildAsync_whenGivenGeneratedCoreAndCookedArtifacts_produces_psp_game_folder" -v minimal`

Expected: FAIL with `NotSupportedException` or missing PSP workspace/executor types.

- [ ] **Step 3: Implement the PSP workspace and layout writer**

```csharp
// builder/PspBuildWorkspace.cs
namespace helengine.psp.builder;

/// <summary>
/// Describes the prepared filesystem layout consumed by the native PSP build.
/// </summary>
public sealed class PspBuildWorkspace {
    /// <summary>
    /// PSP homebrew directory name used by the first staged app.
    /// </summary>
    public const string GameDirectoryName = "HELENGINE";

    /// <summary>
    /// Initializes one PSP build workspace.
    /// </summary>
    public PspBuildWorkspace(
        string repositoryRootPath,
        string stagingRootPath,
        string generatedCoreRootPath,
        string outputRootPath,
        string nativePbpPath) {
        RepositoryRootPath = Path.GetFullPath(repositoryRootPath);
        StagingRootPath = Path.GetFullPath(stagingRootPath);
        GeneratedCoreRootPath = Path.GetFullPath(generatedCoreRootPath);
        OutputRootPath = Path.GetFullPath(outputRootPath);
        NativePbpPath = Path.GetFullPath(nativePbpPath);
    }

    public string RepositoryRootPath { get; }
    public string StagingRootPath { get; }
    public string GeneratedCoreRootPath { get; }
    public string OutputRootPath { get; }
    public string NativePbpPath { get; }
    public string AppRootPath => Path.Combine(OutputRootPath, "PSP", "GAME", GameDirectoryName);
    public string CookedOutputRootPath => Path.Combine(AppRootPath, "cooked");
    public string AppEbootPath => Path.Combine(AppRootPath, "EBOOT.PBP");
}
```

```csharp
// builder/PspAppLayoutWriter.cs
namespace helengine.psp.builder;

/// <summary>
/// Copies staged cooked artifacts into the final PSP homebrew app layout.
/// </summary>
public sealed class PspAppLayoutWriter {
    /// <summary>
    /// Writes the PSP app layout for the prepared workspace.
    /// </summary>
    public void Write(PspBuildWorkspace workspace) {
        Directory.CreateDirectory(workspace.AppRootPath);
        Directory.CreateDirectory(workspace.CookedOutputRootPath);

        string[] stagedFiles = Directory.GetFiles(workspace.StagingRootPath, "*", SearchOption.AllDirectories);
        Array.Sort(stagedFiles, StringComparer.OrdinalIgnoreCase);
        for (int index = 0; index < stagedFiles.Length; index++) {
            string stagedFilePath = stagedFiles[index];
            string relativePath = Path.GetRelativePath(workspace.StagingRootPath, stagedFilePath);
            string destinationPath = Path.Combine(workspace.AppRootPath, relativePath);
            Directory.CreateDirectory(Path.GetDirectoryName(destinationPath)!);
            File.Copy(stagedFilePath, destinationPath, true);
        }

        Directory.CreateDirectory(Path.GetDirectoryName(workspace.AppEbootPath)!);
        File.Copy(workspace.NativePbpPath, workspace.AppEbootPath, true);
    }
}
```

- [ ] **Step 4: Implement the PSP native build executor and builder `BuildAsync(...)`**

```csharp
// builder/IPspNativeBuildExecutor.cs
namespace helengine.psp.builder;

public interface IPspNativeBuildExecutor {
    void Build(PspBuildWorkspace workspace, CancellationToken cancellationToken);
}
```

```csharp
// builder/PspNativeBuildExecutor.cs
using System.Diagnostics;

namespace helengine.psp.builder;

/// <summary>
/// Invokes the native PSP Docker/Make build using the staged generated core root.
/// </summary>
public sealed class PspNativeBuildExecutor : IPspNativeBuildExecutor {
    public void Build(PspBuildWorkspace workspace, CancellationToken cancellationToken) {
        ProcessStartInfo startInfo = new ProcessStartInfo {
            FileName = "docker",
            Arguments = "run --rm -v \"" + workspace.RepositoryRootPath + ":/workspace\" -v \"" + workspace.GeneratedCoreRootPath + ":/generated-core\" -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core",
            UseShellExecute = false
        };

        using Process process = Process.Start(startInfo) ?? throw new InvalidOperationException("Failed to start the PSP native build.");
        process.WaitForExit();
        if (process.ExitCode != 0) {
            throw new InvalidOperationException("The PSP native build failed.");
        }
    }
}
```

```csharp
// builder/PspPlatformAssetBuilder.cs
readonly IPspNativeBuildExecutor NativeBuildExecutor;
readonly PspAppLayoutWriter AppLayoutWriter;

public PspPlatformAssetBuilder()
    : this(new PspNativeBuildExecutor()) {
}

internal PspPlatformAssetBuilder(IPspNativeBuildExecutor nativeBuildExecutor) {
    NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
    AppLayoutWriter = new PspAppLayoutWriter();
    Descriptor = new PlatformBuilderDescriptor(
        "helengine.psp.builder",
        "1.0.0",
        "psp",
        new EngineCompatibilityRange("1.0.0", "999.0.0"),
        new ManifestCompatibilityRange(1, 3),
        ["psp"],
        ["debug"]);
    Definition = PspPlatformDefinitionFactory.Create();
}

public Task<PlatformBuildReport> BuildAsync(
    PlatformBuildRequest request,
    IPlatformBuildProgressReporter progressReporter,
    IPlatformBuildDiagnosticReporter diagnosticReporter,
    CancellationToken cancellationToken) {
    if (string.IsNullOrWhiteSpace(request.GeneratedCoreCppRootPath) || !Directory.Exists(request.GeneratedCoreCppRootPath)) {
        throw new DirectoryNotFoundException("Generated core root was not found for the PSP build.");
    }

    string repositoryRoot = ResolveRepositoryRootPath();
    string nativePbpPath = Path.Combine(repositoryRoot, "build", "EBOOT.PBP");
    string stagingRoot = Path.Combine(request.WorkingRoot, "psp-staging");
    ResetDirectory(stagingRoot);
    Directory.CreateDirectory(stagingRoot);

    foreach (PlatformBuildArtifact artifact in request.Manifest.CookedArtifacts ?? Array.Empty<PlatformBuildArtifact>()) {
        string sourcePath = Path.GetFullPath(Path.Combine(Directory.GetCurrentDirectory(), artifact.RelativePath.Replace('/', Path.DirectorySeparatorChar)));
        string destinationPath = Path.Combine(stagingRoot, artifact.RelativePath.Replace('/', Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(destinationPath)!);
        File.Copy(sourcePath, destinationPath, true);
    }

    PspBuildWorkspace workspace = new(repositoryRoot, stagingRoot, request.GeneratedCoreCppRootPath, request.OutputRoot, nativePbpPath);
    NativeBuildExecutor.Build(workspace, cancellationToken);
    AppLayoutWriter.Write(workspace);

    return Task.FromResult(new PlatformBuildReport(true, [], BuildSucceededSceneOutcomes(request.Manifest.Scenes), BuildSucceededLooseAssetOutcomes(request.Manifest.LooseAssets)));
}
```

- [ ] **Step 5: Run the PSP builder tests**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS with metadata, staging, and executor command tests green.

- [ ] **Step 6: Commit the PSP builder execution flow**

```bash
rtk git add builder builder.tests
rtk git commit -m "Add PSP builder execution flow"
```

### Task 4: Compile Generated Core Into The PSP Native Player

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `Makefile`
- Modify: `README.md`

- [ ] **Step 1: Write the failing PSP build-system test in the builder smoke path**

```csharp
// builder/Program.cs
if (args.Length > 0 && string.Equals(args[0], "--smoke-test", StringComparison.OrdinalIgnoreCase)) {
    string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-psp-builder-smoke-" + Guid.NewGuid().ToString("N"));
    string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
    Directory.CreateDirectory(generatedCoreRoot);
    File.WriteAllText(Path.Combine(generatedCoreRoot, "helengine_core_amalgamated.cpp"), "// generated");
    File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "#pragma once");
    Console.WriteLine("Smoke test prepared generated-core prerequisites.");
    return 0;
}
```

Run: `rtk dotnet run --project builder/helengine.psp.builder.csproj -- --smoke-test`

Expected: FAIL later, once the smoke test asserts the native build must require generated core and produce `build/EBOOT.PBP`.

- [ ] **Step 2: Update CMake to require and compile generated core**

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.11)

project(helengine_psp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if("${HELENGINE_CORE_CPP_ROOT}" STREQUAL "")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT must point at the generated helengine.core C++ output folder.")
endif()

if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/helcpp_config.hpp")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain helcpp_config.hpp. Regenerate helengine.core C++ output first.")
endif()

if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/helengine_core_amalgamated.cpp")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain helengine_core_amalgamated.cpp. Regenerate helengine.core C++ output first.")
endif()

set(HELENGINE_PSP_SOURCES
    src/main.cpp
    src/platform/psp/PspBootHost.cpp
    src/platform/psp/PspAppRootPathResolver.cpp
    src/platform/psp/PspInputBackend.cpp
    src/platform/psp/PspPackagedAssetLoader.cpp
    src/platform/psp/PspRuntimeSceneCatalogFactory.cpp
    src/platform/psp/rendering/PspRenderManager2D.cpp
    src/platform/psp/rendering/PspRenderManager3D.cpp
    "${HELENGINE_CORE_CPP_ROOT}/helengine_core_amalgamated.cpp"
    "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_startup_manifest.cpp"
    "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_scene_catalog_manifest.cpp"
)

if(EXISTS "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_code_module_manifest.cpp")
    list(APPEND HELENGINE_PSP_SOURCES "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_code_module_manifest.cpp")
endif()

add_executable(${PROJECT_NAME} ${HELENGINE_PSP_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src ${HELENGINE_CORE_CPP_ROOT})
target_link_libraries(${PROJECT_NAME} PRIVATE pspdisplay pspge pspgu pspgum pspctrl pspsdk pspuser)

create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE "Helengine PSP"
    VERSION 01.00
)
```

- [ ] **Step 3: Update Makefile to make the generated-core contract explicit**

```makefile
HELENGINE_CORE_CPP_ROOT ?=

ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
$(error HELENGINE_CORE_CPP_ROOT must point at the generated helengine.core C++ output folder)
endif

BUILD_DIR := build
TARGET_PBP := $(BUILD_DIR)/EBOOT.PBP
CMAKE_ARGS := -DHELENGINE_CORE_CPP_ROOT=$(HELENGINE_CORE_CPP_ROOT)

.PHONY: all clean

all: $(TARGET_PBP)

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && psp-cmake $(CMAKE_ARGS) ..

$(TARGET_PBP): $(BUILD_DIR)/CMakeCache.txt
	$(MAKE) -C $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)
```

- [ ] **Step 4: Document the editor-driven PSP build flow**

```md
## End-to-end PSP build

The real PSP build is editor-driven. The editor regenerates generated-core C++ for platform `psp`, cooks the selected scene/assets, and then invokes the PSP builder assembly.

The native PSP build still compiles in this repository, but it requires `HELENGINE_CORE_CPP_ROOT` to point at the generated core output produced by the editor build graph.

Expected staged output:

- `output/psp/PSP/GAME/HELENGINE/EBOOT.PBP`
- `output/psp/PSP/GAME/HELENGINE/cooked/scenes/rendering/cube_test.hasset`
```

- [ ] **Step 5: Run the PSP builder smoke test and native configure pass**

Run: `rtk dotnet run --project builder/helengine.psp.builder.csproj -- --smoke-test`

Expected: PASS once the smoke test validates that the builder can stage generated-core input and that the native build contract points at `build/EBOOT.PBP`.

- [ ] **Step 6: Commit the generated-core native build integration**

```bash
rtk git add CMakeLists.txt Makefile README.md builder/Program.cs
rtk git commit -m "Require generated core in PSP native build"
```

### Task 5: Boot Generated Core And Load The Startup Scene From The PSP App Root

**Files:**
- Modify: `src/platform/psp/PspBootHost.hpp`
- Modify: `src/platform/psp/PspBootHost.cpp`
- Create: `src/platform/psp/PspAppRootPathResolver.hpp`
- Create: `src/platform/psp/PspAppRootPathResolver.cpp`
- Create: `src/platform/psp/PspInputBackend.hpp`
- Create: `src/platform/psp/PspInputBackend.cpp`
- Create: `src/platform/psp/PspPackagedAssetLoader.hpp`
- Create: `src/platform/psp/PspPackagedAssetLoader.cpp`
- Create: `src/platform/psp/PspRuntimeSceneCatalogFactory.hpp`
- Create: `src/platform/psp/PspRuntimeSceneCatalogFactory.cpp`

- [ ] **Step 1: Write the failing runtime startup-path smoke test**

```csharp
// builder.tests/PspPlatformAssetBuilderTests.cs
[Fact]
public async Task BuildAsync_smoke_test_requires_startup_scene_and_generated_core() {
    string workingRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
    string outputRoot = Path.Combine(workingRoot, "out");
    string sourceRoot = Path.Combine(workingRoot, "staging");
    Directory.CreateDirectory(sourceRoot);

    PlatformBuildManifest manifest = new(
        3,
        "city",
        "1.0.0",
        "1.0.0",
        "scenes/rendering/cube_test.helen",
        Array.Empty<PlatformBuildScene>(),
        Array.Empty<PlatformBuildAsset>(),
        Array.Empty<PlatformBuildArtifact>(),
        Array.Empty<PlatformBuildCodeModule>(),
        Array.Empty<PlatformArtifactPlacement>(),
        new PlatformContainerWritePlan("psp-homebrew", Array.Empty<PlatformContainerArtifact>()));

    PlatformBuildRequest request = new(
        manifest,
        [new PlatformBuildTargetVariant("psp-debug", "psp", "psp", "debug")],
        [new PlatformCookProfile("default", "Default", new PlatformCookProfileCapabilities("psp", "raw", "rgba", "psp-scene-v1", PlatformSerializationEndianness.LittleEndian))],
        outputRoot,
        Path.Combine(workingRoot, "tmp"),
        selectedBuildProfileId: "debug",
        selectedGraphicsProfileId: "psp-forward",
        selectedCodegenProfileId: "default",
        selectedBuildOptionValues: new Dictionary<string, string>(),
        selectedGraphicsOptionValues: new Dictionary<string, string>(),
        selectedCodegenOptionValues: new Dictionary<string, string>(),
        generatedCoreCppRootPath: string.Empty,
        selectedMediaProfileId: "psp-game-folder",
        selectedStorageProfileId: "homebrew-app");

    await Assert.ThrowsAsync<DirectoryNotFoundException>(() =>
        new PspPlatformAssetBuilder(new RecordingPspNativeBuildExecutor()).BuildAsync(
            request,
            new RecordingProgressReporter(),
            new RecordingDiagnosticReporter(),
            CancellationToken.None));
}
```

- [ ] **Step 2: Run the builder tests and verify the runtime prerequisites still fail**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~BuildAsync_smoke_test_requires_startup_scene_and_generated_core" -v minimal`

Expected: PASS once the builder rejects missing generated-core input and missing scene metadata clearly.

- [ ] **Step 3: Add PSP app-root, packaged-asset, and scene-catalog bridge helpers**

```cpp
// src/platform/psp/PspAppRootPathResolver.hpp
#pragma once

#include <string>

namespace helengine::psp {
    /// Resolves the running PSP homebrew app directory that owns EBOOT.PBP and cooked assets.
    class PspAppRootPathResolver {
    public:
        /// Resolves the PSP homebrew app root path.
        std::string ResolveAppRootPath() const;
    };
}
```

```cpp
// src/platform/psp/PspPackagedAssetLoader.hpp
#pragma once

#include <string>

class Asset;
class SceneAsset;

namespace helengine::psp {
    /// Loads cooked packaged assets from the staged PSP homebrew app directory.
    class PspPackagedAssetLoader {
    public:
        /// Creates the packaged asset loader for the supplied app root.
        explicit PspPackagedAssetLoader(const std::string& appRootPath);

        /// Loads one serialized packaged asset by cooked-relative path.
        Asset* LoadAsset(const std::string& cookedRelativePath) const;

        /// Loads the configured startup scene using the generated startup manifest.
        SceneAsset* LoadStartupScene() const;

    private:
        /// Absolute PSP app root that contains the cooked asset tree.
        std::string AppRootPath;
    };
}
```

```cpp
// src/platform/psp/PspRuntimeSceneCatalogFactory.hpp
#pragma once

class RuntimeSceneCatalog;

namespace helengine::psp {
    /// Builds a generated-core runtime scene catalog from the generated native scene manifest.
    class PspRuntimeSceneCatalogFactory {
    public:
        /// Builds the runtime scene catalog instance consumed by generated core.
        RuntimeSceneCatalog* Build() const;
    };
}
```

- [ ] **Step 4: Initialize generated core and load the startup scene in `PspBootHost`**

```cpp
// src/platform/psp/PspBootHost.hpp
#pragma once

class Core;
class CoreInitializationOptions;
class RenderManager2D;
class RenderManager3D;
class IInputBackend;

namespace helengine::psp {
    class PspBootHost {
    public:
        PspBootHost();
        int Run();

    private:
        bool InitializeGraphics();
        bool InitializeEngine();
        void PresentFrame();

        unsigned int* DisplayList;
        Core* EngineCore;
        CoreInitializationOptions* EngineOptions;
        RenderManager3D* EngineRenderManager3D;
        RenderManager2D* EngineRenderManager2D;
        IInputBackend* EngineInputBackend;
    };
}
```

```cpp
// src/platform/psp/PspBootHost.cpp
#include "platform/psp/PspAppRootPathResolver.hpp"
#include "platform/psp/PspInputBackend.hpp"
#include "platform/psp/PspPackagedAssetLoader.hpp"
#include "platform/psp/PspRuntimeSceneCatalogFactory.hpp"
#include "platform/psp/rendering/PspRenderManager2D.hpp"
#include "platform/psp/rendering/PspRenderManager3D.hpp"
#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "SceneAsset.hpp"

int PspBootHost::Run() {
    if (!InitializeGraphics()) {
        return 1;
    }
    if (!InitializeEngine()) {
        return 1;
    }

    while (true) {
        EngineCore->Update();
        EngineCore->Draw();
        PresentFrame();
    }
}

bool PspBootHost::InitializeEngine() {
    PspAppRootPathResolver appRootResolver;
    std::string appRootPath = appRootResolver.ResolveAppRootPath();

    EngineCore = new Core();
    EngineOptions = EngineCore->get_InitializationOptions();
    EngineOptions->ContentRootPath = appRootPath;
    EngineOptions->UpdateOrderLayers = 4;
    EngineOptions->RenderOrderLayers3D = 4;
    EngineOptions->UpdateListInitialCapacity = 64;
    EngineOptions->RenderList2DInitialCapacity = 8;
    EngineOptions->RenderList3DInitialCapacity = 64;
    EngineOptions->SceneCatalog = PspRuntimeSceneCatalogFactory().Build();

    EngineRenderManager3D = new PspRenderManager3D(DisplayList);
    EngineRenderManager2D = new PspRenderManager2D();
    EngineInputBackend = new PspInputBackend();

    EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputBackend, EngineOptions);

    PspPackagedAssetLoader assetLoader(appRootPath);
    SceneAsset* startupScene = assetLoader.LoadStartupScene();
    EngineCore->get_SceneLoadService()->Load(startupScene);
    return true;
}
```

- [ ] **Step 5: Run the PSP builder tests and the native compile smoke path**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS.

Run: `rtk dotnet run --project builder/helengine.psp.builder.csproj -- --smoke-test`

Expected: PASS once the builder smoke test validates startup scene and generated-core prerequisites.

- [ ] **Step 6: Commit the PSP generated-core boot path**

```bash
rtk git add src/platform/psp builder builder.tests
rtk git commit -m "Boot PSP player from generated core"
```

### Task 6: Add The Minimal PSP 3D Render Path And Verify The City Build

**Files:**
- Create: `src/platform/psp/rendering/PspRenderManager2D.hpp`
- Create: `src/platform/psp/rendering/PspRenderManager2D.cpp`
- Create: `src/platform/psp/rendering/PspRenderManager3D.hpp`
- Create: `src/platform/psp/rendering/PspRenderManager3D.cpp`
- Modify: `src/platform/psp/PspBootHost.cpp`
- Modify: `/mnt/c/dev/helprojs/city/user_settings/build_config.json` (local verification only; do not commit unless explicitly requested)

- [ ] **Step 1: Write the failing local verification PSP build-config entry**

```json
// /mnt/c/dev/helprojs/city/user_settings/build_config.json
{
  "platformId": "psp",
  "selectedSceneIds": [
    "scenes/rendering/cube_test.helen"
  ],
  "sceneOrders": [
    {
      "sceneId": "scenes/rendering/cube_test.helen",
      "orderNumber": 1
    }
  ],
  "outputDirectoryPath": "C:\\dev\\helprojs\\output\\psp",
  "debugBuild": true,
  "selectedBuildProfileId": "debug",
  "selectedGraphicsProfileId": "psp-forward",
  "selectedBuildOptionValues": {
    "texture-scale-percent": "100",
    "shader-variant-pruning": "true"
  },
  "selectedGraphicsOptionValues": {
    "default-width": "480",
    "default-height": "272",
    "vsync-enabled": "true",
    "fullscreen-enabled": "true"
  },
  "selectedCodegenProfileId": "default",
  "selectedStorageProfileId": "homebrew-app",
  "selectedMediaProfileId": "psp-game-folder",
  "selectedCodegenOptionValues": {
    "write-conversion-report": "true",
    "include-project-defined-preprocessor-symbols": "false",
    "load-native-runtime-metadata": "true"
  },
  "selectedCodeModuleIds": []
}
```

- [ ] **Step 2: Implement the minimal PSP render managers**

```cpp
// src/platform/psp/rendering/PspRenderManager2D.hpp
#pragma once

#include "RenderManager2D.hpp"

namespace helengine::psp {
    /// No-op 2D render manager used during the first PSP 3D-only bring-up.
    class PspRenderManager2D final : public RenderManager2D {
    public:
        /// Draws a sprite. The first PSP milestone does not support 2D output.
        void DrawSprite(ISpriteDrawable2D* sprite) override;

        /// Draws text. The first PSP milestone does not support text output.
        void DrawText(ITextDrawable2D* text) override;
    };
}
```

```cpp
// src/platform/psp/rendering/PspRenderManager3D.hpp
#pragma once

#include "RenderManager3D.hpp"

namespace helengine::psp {
    /// Minimal PSP 3D render manager that submits the authored cube mesh as an unlit GU triangle list.
    class PspRenderManager3D final : public RenderManager3D {
    public:
        /// Creates the PSP render manager bound to the supplied GU display list.
        explicit PspRenderManager3D(unsigned int* displayList);

        /// Draws one 3D drawable through the PSP GU path.
        void Draw(IDrawable3D* drawable) override;

    private:
        /// GU display list used for per-frame draw submission.
        unsigned int* DisplayList;
    };
}
```

```cpp
// src/platform/psp/rendering/PspRenderManager3D.cpp
#include "platform/psp/rendering/PspRenderManager3D.hpp"

#include <pspgu.h>
#include <pspgum.h>

#include "Entity.hpp"
#include "IDrawable3D.hpp"

namespace helengine::psp {
    PspRenderManager3D::PspRenderManager3D(unsigned int* displayList)
        : DisplayList(displayList) {
    }

    void PspRenderManager3D::Draw(IDrawable3D* drawable) {
        if (drawable == nullptr) {
            return;
        }

        Entity* parent = drawable->get_Parent();
        if (parent == nullptr) {
            return;
        }

        RuntimeModel* runtimeModel = drawable->get_Model();
        if (runtimeModel == nullptr) {
            return;
        }

        // Use the generated runtime model mesh buffers, the parent entity transform,
        // and the active camera matrices to submit opaque unlit GU triangles here.
    }
}
```

- [ ] **Step 3: Make `PspBootHost` present the render-manager-driven frame**

```cpp
void PspBootHost::PresentFrame() {
    sceGuStart(GU_DIRECT, DisplayList);
    sceGuClearColor(0xFF101018);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}
```

Then expand `PspRenderManager3D::Draw(...)` to:

```cpp
- read the generated runtime model mesh for the cube
- compute the world transform from the generated-core entity transform
- configure GU projection/view/model matrices from the active camera
- submit the cube as opaque unlit triangles
```

- [ ] **Step 4: Run the builder tests and the city PSP build**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS.

Run: `rtk dotnet build /mnt/c/dev/helworks/helengine/helengine.ui/helengine.editor.app/helengine.editor.app.csproj -c Debug -v minimal`

Expected: PASS.

Run: `rtk dotnet /mnt/c/dev/helworks/helengine/helengine.ui/helengine.editor.app/bin/Debug/net9.0-windows/helengine.editor.app.dll --project "C:\\dev\\helprojs\\city\\project.heproj" --build psp --output "C:\\dev\\helprojs\\output\\psp"`

Expected: PASS with a staged PSP homebrew app under `C:\dev\helprojs\output\psp\PSP\GAME\HELENGINE`.

- [ ] **Step 5: Verify the staged PPSSPP output manually**

Run:

```bash
rtk rg --files /mnt/c/dev/helprojs/output/psp/PSP/GAME/HELENGINE
```

Expected output includes:

```text
/mnt/c/dev/helprojs/output/psp/PSP/GAME/HELENGINE/EBOOT.PBP
/mnt/c/dev/helprojs/output/psp/PSP/GAME/HELENGINE/cooked/scenes/rendering/cube_test.hasset
```

Then open `EBOOT.PBP` in PPSSPP and confirm:

- boot succeeds
- the authored `cube_test` scene loads
- the cube appears
- the cube rotates through the normal runtime/script path

- [ ] **Step 6: Commit the minimal PSP runtime renderer**

```bash
rtk git add src/platform/psp
rtk git commit -m "Render city cube test on PSP"
```
