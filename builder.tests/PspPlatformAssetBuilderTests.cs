using helengine.baseplatform.Manifest;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.baseplatform.Results;
using helengine;

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
        Assert.Contains(builder.Definition.ComponentSupportRules, supportRule =>
            supportRule.ComponentTypeId == "helengine.meshcomponent");
        Assert.Contains(builder.Definition.ComponentSupportRules, supportRule =>
            supportRule.ComponentTypeId == "helengine.cameracomponent");
        Assert.Contains(builder.Definition.ComponentSupportRules, supportRule =>
            supportRule.ComponentTypeId == "helengine.directionalshadowtowerspincomponent");
        Assert.Contains(builder.Definition.ComponentSupportRules, supportRule =>
            supportRule.ComponentTypeId == "city.menu.demodiscreturntomenucomponent, gameplay");
    }

    /// <summary>
    /// Verifies the PSP material schema mirrors the standard material contract used by the editor build graph.
    /// </summary>
    [Fact]
    public void Descriptor_and_definition_expose_standard_material_fields() {
        PspPlatformAssetBuilder builder = new();

        PlatformMaterialSchemaDefinition schema = Assert.Single(builder.Definition.MaterialSchemas, materialSchema => materialSchema.SchemaId == "standard-shader");

        Assert.Collection(schema.Fields,
            field => {
                Assert.Equal("use-custom-shader", field.FieldId);
                Assert.Equal(PlatformMaterialFieldKind.Boolean, field.FieldKind);
                Assert.Equal("false", field.DefaultValue);
                Assert.True(field.Required);
            },
            field => {
                Assert.Equal("shader-asset-id", field.FieldId);
                Assert.Equal(PlatformMaterialFieldKind.AssetReference, field.FieldKind);
                Assert.Equal(string.Empty, field.DefaultValue);
                Assert.True(field.Required);
            },
            field => {
                Assert.Equal("vertex-program", field.FieldId);
                Assert.Equal(PlatformMaterialFieldKind.Text, field.FieldKind);
                Assert.Equal(string.Empty, field.DefaultValue);
                Assert.True(field.Required);
            },
            field => {
                Assert.Equal("pixel-program", field.FieldId);
                Assert.Equal(PlatformMaterialFieldKind.Text, field.FieldKind);
                Assert.Equal(string.Empty, field.DefaultValue);
                Assert.True(field.Required);
            },
            field => {
                Assert.Equal("base-color", field.FieldId);
                Assert.Equal(PlatformMaterialFieldKind.Color, field.FieldKind);
                Assert.Equal("#ffffff", field.DefaultValue);
                Assert.False(field.Required);
            },
            field => {
                Assert.Equal("lighting-response", field.FieldId);
                Assert.Equal(PlatformMaterialFieldKind.Text, field.FieldKind);
                Assert.Equal("lit-directional", field.DefaultValue);
                Assert.False(field.Required);
            },
            field => {
                Assert.Equal("receives-lighting", field.FieldId);
                Assert.Equal(PlatformMaterialFieldKind.Boolean, field.FieldKind);
                Assert.Equal("true", field.DefaultValue);
                Assert.False(field.Required);
            },
            field => {
                Assert.Equal("texture-id", field.FieldId);
                Assert.Equal(PlatformMaterialFieldKind.AssetReference, field.FieldKind);
                Assert.Equal(string.Empty, field.DefaultValue);
                Assert.False(field.Required);
            },
            field => {
                Assert.Equal("casts-shadow", field.FieldId);
                Assert.Equal(PlatformMaterialFieldKind.Boolean, field.FieldKind);
                Assert.Equal("true", field.DefaultValue);
                Assert.False(field.Required);
            },
            field => {
                Assert.Equal("receives-shadow", field.FieldId);
                Assert.Equal(PlatformMaterialFieldKind.Boolean, field.FieldKind);
                Assert.Equal("true", field.DefaultValue);
                Assert.False(field.Required);
            });
    }

    /// <summary>
    /// Verifies the PSP material schema exposes lighting intent fields that stay independent from the renderer backend.
    /// </summary>
    [Fact]
    public void Descriptor_and_definition_expose_psp_lighting_fields() {
        PspPlatformAssetBuilder builder = new();

        PlatformMaterialSchemaDefinition schema = Assert.Single(
            builder.Definition.MaterialSchemas,
            materialSchema => materialSchema.SchemaId == "standard-shader");

        Assert.Contains(schema.Fields, field =>
            field.FieldId == "lighting-response"
            && field.FieldKind == PlatformMaterialFieldKind.Text
            && field.DefaultValue == "lit-directional");
        Assert.Contains(schema.Fields, field =>
            field.FieldId == "receives-lighting"
            && field.FieldKind == PlatformMaterialFieldKind.Boolean
            && field.DefaultValue == "true");
    }

    /// <summary>
    /// Verifies the PSP material cook path preserves the standard material asset data required by the runtime resolver.
    /// </summary>
    [Fact]
    public void CookMaterial_preserves_diffuse_texture_and_shadow_fields() {
        PspPlatformAssetBuilder builder = new();

        PlatformMaterialCookResult result = builder.CookMaterial(new PlatformMaterialCookRequest(
            "Materials/Test.helmat",
            "Materials/Test.helmat",
            "psp",
            "debug",
            "psp-forward",
            "standard-shader",
            new Dictionary<string, string> {
                ["use-custom-shader"] = "false",
                ["shader-asset-id"] = "engine:material:standard",
                ["vertex-program"] = "ForwardStandard.vs",
                ["pixel-program"] = "ForwardStandard.ps",
                ["variant"] = "Mesh",
                ["base-color"] = "#336699",
                ["texture-id"] = "Textures/Checker",
                ["casts-shadow"] = "false",
                ["receives-shadow"] = "true"
            }));

        MaterialAsset materialAsset = Assert.IsType<MaterialAsset>(AssetSerializer.DeserializeFromBytes(result.CookedMaterialBytes));
        Assert.Equal("engine:material:standard", materialAsset.ShaderAssetId);
        Assert.Equal("Textures/Checker", materialAsset.DiffuseTextureAssetId);
        Assert.False(materialAsset.CastsShadows);
        Assert.True(materialAsset.ReceivesShadows);
        MaterialConstantBufferAsset baseColorBuffer = Assert.Single(materialAsset.ConstantBuffers, buffer => buffer.Name == "BaseColorBuffer");
        Assert.Equal(16, baseColorBuffer.Data.Length);
        Assert.Equal(new[] { "engine:material:standard" }, result.ReferencedShaderAssetIds);
    }

    /// <summary>
    /// Verifies the PSP material cook path preserves lighting intent in a dedicated constant buffer.
    /// </summary>
    [Fact]
    public void CookMaterial_preserves_psp_lighting_configuration() {
        PspPlatformAssetBuilder builder = new();

        PlatformMaterialCookResult result = builder.CookMaterial(new PlatformMaterialCookRequest(
            "Materials/Test.helmat",
            "Materials/Test.helmat",
            "psp",
            "debug",
            "psp-forward",
            "standard-shader",
            new Dictionary<string, string> {
                ["use-custom-shader"] = "false",
                ["shader-asset-id"] = "engine:material:standard",
                ["vertex-program"] = "ForwardStandard.vs",
                ["pixel-program"] = "ForwardStandard.ps",
                ["variant"] = "Mesh",
                ["base-color"] = "#336699",
                ["lighting-response"] = "unlit",
                ["receives-lighting"] = "false",
                ["texture-id"] = string.Empty,
                ["casts-shadow"] = "false",
                ["receives-shadow"] = "true"
            }));

        MaterialAsset materialAsset = Assert.IsType<MaterialAsset>(AssetSerializer.DeserializeFromBytes(result.CookedMaterialBytes));
        MaterialConstantBufferAsset lightingBuffer = Assert.Single(materialAsset.ConstantBuffers, buffer => buffer.Name == "LightingConfigBuffer");

        Assert.Equal(16, lightingBuffer.Data.Length);
        Assert.Equal(0.0f, BitConverter.ToSingle(lightingBuffer.Data, 0));
        Assert.Equal(0.0f, BitConverter.ToSingle(lightingBuffer.Data, 4));
        Assert.Equal(0.0f, BitConverter.ToSingle(lightingBuffer.Data, 8));
        Assert.Equal(0.0f, BitConverter.ToSingle(lightingBuffer.Data, 12));
    }

    /// <summary>
    /// Verifies the PSP base-color parser preserves the documented <c>#RRGGBBAA</c> channel order.
    /// </summary>
    [Fact]
    public void CookMaterial_preserves_rgba_base_color_order() {
        PspPlatformAssetBuilder builder = new();

        PlatformMaterialCookResult result = builder.CookMaterial(new PlatformMaterialCookRequest(
            "Materials/Test.helmat",
            "Materials/Test.helmat",
            "psp",
            "debug",
            "psp-forward",
            "standard-shader",
            new Dictionary<string, string> {
                ["use-custom-shader"] = "false",
                ["shader-asset-id"] = "engine:material:standard",
                ["vertex-program"] = "ForwardStandard.vs",
                ["pixel-program"] = "ForwardStandard.ps",
                ["variant"] = "Mesh",
                ["base-color"] = "#FF404080",
                ["texture-id"] = string.Empty,
                ["casts-shadow"] = "true",
                ["receives-shadow"] = "true"
            }));

        MaterialAsset materialAsset = Assert.IsType<MaterialAsset>(AssetSerializer.DeserializeFromBytes(result.CookedMaterialBytes));
        MaterialConstantBufferAsset baseColorBuffer = Assert.Single(materialAsset.ConstantBuffers, buffer => buffer.Name == "BaseColorBuffer");

        Assert.Equal(1.0f, BitConverter.ToSingle(baseColorBuffer.Data, 0));
        Assert.Equal(64.0f / 255.0f, BitConverter.ToSingle(baseColorBuffer.Data, 4), 5);
        Assert.Equal(64.0f / 255.0f, BitConverter.ToSingle(baseColorBuffer.Data, 8), 5);
        Assert.Equal(128.0f / 255.0f, BitConverter.ToSingle(baseColorBuffer.Data, 12), 5);
    }

    /// <summary>
    /// Verifies the PSP builder stages cooked artifacts into a PSP homebrew game folder and copies the built PBP into place.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenGivenGeneratedCoreAndCookedArtifacts_produces_psp_game_folder() {
        string workingRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string sourceRoot = Path.Combine(workingRoot, "staging");
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string generatedCoreRuntimeRoot = Path.Combine(generatedCoreRoot, "runtime");
        string repositoryRoot = Path.Combine(workingRoot, "repository");
        string sceneSourcePath = Path.Combine(sourceRoot, "cooked", "scenes", "rendering", "cube_test.hasset");
        string modelSourcePath = Path.Combine(sourceRoot, "cooked", "models", "cube.hasset");
        string materialSourcePath = Path.Combine(sourceRoot, "cooked", "materials", "standard.hasset");
        string importedTextureSourcePath = Path.Combine(sourceRoot, "cooked", "imported", "textures", "checker");

        Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath)!);
        Directory.CreateDirectory(Path.GetDirectoryName(modelSourcePath)!);
        Directory.CreateDirectory(Path.GetDirectoryName(materialSourcePath)!);
        Directory.CreateDirectory(Path.GetDirectoryName(importedTextureSourcePath)!);
        Directory.CreateDirectory(generatedCoreRoot);
        Directory.CreateDirectory(generatedCoreRuntimeRoot);
        Directory.CreateDirectory(Path.Combine(repositoryRoot, "src", "platform", "psp"));
        File.WriteAllText(sceneSourcePath, "scene payload");
        File.WriteAllText(modelSourcePath, "model payload");
        File.WriteAllText(materialSourcePath, "material payload");
        File.WriteAllText(importedTextureSourcePath, "texture payload");
        File.WriteAllText(Path.Combine(generatedCoreRoot, "helengine_core_amalgamated.cpp"), "// generated");
        File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "#pragma once");
        File.WriteAllText(Path.Combine(generatedCoreRuntimeRoot, "runtime_startup_manifest.cpp"), "// startup");
        File.WriteAllText(Path.Combine(generatedCoreRuntimeRoot, "runtime_scene_catalog_manifest.cpp"), "// scene catalog");
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
                    new PlatformBuildArtifact("cooked/materials/standard.hasset", "material:standard", "sha256:material", "material", "shared"),
                    new PlatformBuildArtifact("cooked/imported/textures/checker", "texture:checker", "sha256:texture", "texture", "shared")
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
                selectedGraphicsOptionValues: new Dictionary<string, string>(),
                selectedCodegenOptionValues: new Dictionary<string, string>(),
                generatedCoreCppRootPath: generatedCoreRoot,
                selectedMediaProfileId: "psp-game-folder",
                selectedStorageProfileId: "homebrew-app");

            FakePspNativeBuildExecutor nativeBuildExecutor = new();
            PspPlatformAssetBuilder builder = new(nativeBuildExecutor);
            RecordingProgressReporter progressReporter = new();
            RecordingDiagnosticReporter diagnosticReporter = new();

            PlatformBuildReport report = await builder.BuildAsync(
                request,
                progressReporter,
                diagnosticReporter,
                CancellationToken.None);

            Assert.True(report.Succeeded);
            Assert.Empty(diagnosticReporter.Diagnostics);
            Assert.Equal(4, progressReporter.Updates.Count);
            Assert.True(File.Exists(Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "EBOOT.PBP")));
            Assert.True(File.Exists(Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "cooked", "scenes", "rendering", "cube_test.hasset")));
            Assert.True(File.Exists(Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "cooked", "models", "cube.hasset")));
            Assert.True(File.Exists(Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "cooked", "materials", "standard.hasset")));
            Assert.True(File.Exists(Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "cooked", "imported", "textures", "checker")));
            Assert.Equal(generatedCoreRoot, nativeBuildExecutor.LastWorkspace.GeneratedCoreRootPath);
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

    /// <summary>
    /// Verifies the PSP builder rejects requests that do not include a loadable startup scene inside the cooked scene manifest.
    /// </summary>
    [Fact]
    public async Task BuildAsync_requires_startup_scene_to_exist_in_scene_manifest() {
        string workingRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string sourceRoot = Path.Combine(workingRoot, "staging");
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string repositoryRoot = Path.Combine(workingRoot, "repository");
        string sceneSourcePath = Path.Combine(sourceRoot, "cooked", "scenes", "rendering", "cube_test.hasset");

        Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath)!);
        Directory.CreateDirectory(generatedCoreRoot);
        Directory.CreateDirectory(Path.Combine(generatedCoreRoot, "runtime"));
        Directory.CreateDirectory(Path.Combine(repositoryRoot, "src", "platform", "psp"));
        File.WriteAllText(sceneSourcePath, "scene payload");
        File.WriteAllText(Path.Combine(generatedCoreRoot, "helengine_core_amalgamated.cpp"), "// generated");
        File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "#pragma once");
        File.WriteAllText(Path.Combine(generatedCoreRoot, "runtime", "runtime_startup_manifest.cpp"), "// startup");
        File.WriteAllText(Path.Combine(generatedCoreRoot, "runtime", "runtime_scene_catalog_manifest.cpp"), "// scene catalog");
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
                "scenes/rendering/cube_test.helen",
                Array.Empty<PlatformBuildScene>(),
                Array.Empty<PlatformBuildAsset>(),
                [
                    new PlatformBuildArtifact("cooked/scenes/rendering/cube_test.hasset", "scene:cube-test", "sha256:scene", "scene", "shared")
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
                selectedGraphicsOptionValues: new Dictionary<string, string>(),
                selectedCodegenOptionValues: new Dictionary<string, string>(),
                generatedCoreCppRootPath: generatedCoreRoot,
                selectedMediaProfileId: "psp-game-folder",
                selectedStorageProfileId: "homebrew-app");

            PspPlatformAssetBuilder builder = new(new FakePspNativeBuildExecutor());

            InvalidOperationException exception = await Assert.ThrowsAsync<InvalidOperationException>(() =>
                builder.BuildAsync(
                    request,
                    new RecordingProgressReporter(),
                    new RecordingDiagnosticReporter(),
                    CancellationToken.None));

            Assert.Contains("startup scene", exception.Message, StringComparison.OrdinalIgnoreCase);
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
}
