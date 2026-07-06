using helengine.baseplatform.Manifest;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.baseplatform.Results;
using helengine;
using helengine.editor;
using System.Reflection;

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
            supportRule.ComponentTypeId == "gameplay.rendering.directionalshadowtowerspincomponent, gameplay");
        Assert.Contains(builder.Definition.ComponentSupportRules, supportRule =>
            supportRule.ComponentTypeId == "city.menu.demodiscreturntomenucomponent, gameplay");
        Assert.Contains(builder.Definition.AssetCookCapabilities, capability =>
            capability.SourceAssetKind == "texture"
            && capability.TargetArtifactKind == "runtime-texture"
            && capability.OwnershipKind == PlatformAssetCookOwnershipKind.BuilderOwned);
        Assert.Contains(builder.Definition.AssetCookCapabilities, capability =>
            capability.SourceAssetKind == "font-atlas-texture"
            && capability.TargetArtifactKind == "runtime-texture"
            && capability.OwnershipKind == PlatformAssetCookOwnershipKind.BuilderOwned);
    }

    /// <summary>
    /// Ensures the PSP builder publishes generic texture-format capability metadata for both image textures and font atlas textures.
    /// </summary>
    [Fact]
    public void Descriptor_and_definition_expose_texture_format_capabilities() {
        PspPlatformAssetBuilder builder = new();

        Assert.Collection(
            builder.Definition.AssetCookCapabilities,
            capability => {
                Assert.Equal("texture", capability.SourceAssetKind);
                Assert.Equal("runtime-texture", capability.TargetArtifactKind);
                Assert.Equal(PlatformAssetCookOwnershipKind.BuilderOwned, capability.OwnershipKind);
                Assert.Equal("psp-texture", capability.SettingsContractId);
                Assert.Equal("{\"maxResolution\":512,\"colorFormat\":\"Rgba4444\",\"alphaPrecision\":\"A4\"}", capability.DefaultSerializedPlatformSettings);
                AssertTextureFormatCapabilities(capability.TextureFormatCapabilities);
            },
            capability => {
                Assert.Equal("font-atlas-texture", capability.SourceAssetKind);
                Assert.Equal("runtime-texture", capability.TargetArtifactKind);
                Assert.Equal(PlatformAssetCookOwnershipKind.BuilderOwned, capability.OwnershipKind);
                Assert.Equal("psp-font-atlas-texture", capability.SettingsContractId);
                Assert.Equal("{\"maxResolution\":0,\"colorFormat\":\"Indexed8\",\"alphaPrecision\":\"A8\"}", capability.DefaultSerializedPlatformSettings);
                AssertTextureFormatCapabilities(capability.TextureFormatCapabilities);
            });
    }

    /// <summary>
    /// Ensures the PSP default codegen profile publishes the generic native C++ platform-shape options required by the shared generator.
    /// </summary>
    [Fact]
    public void Default_codegen_profile_exposes_required_custom_cpp_platform_shape_options() {
        PspPlatformAssetBuilder builder = new();
        PlatformCodegenProfileDefinition codegenProfile = Assert.Single(
            builder.Definition.CodegenProfiles,
            profile => profile.ProfileId == "default");

        Assert.Contains(
            codegenProfile.Settings,
            setting => setting.SettingId == "generated-math-convention"
                && setting.DefaultValue == "native-column-vector");
        Assert.Contains(
            codegenProfile.Settings,
            setting => setting.SettingId == "pointer-size-bytes"
                && setting.DefaultValue == "4");
        Assert.Contains(
            codegenProfile.Settings,
            setting => setting.SettingId == "type-remaps"
                && setting.DefaultValue == "System.Numerics.Vector2=helengine.float2;System.Numerics.Vector3=helengine.float3;System.Numerics.Vector4=helengine.float4;System.Numerics.Quaternion=helengine.float4");
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
    /// Verifies one PSP texture cook capability advertises the expected supported formats and valid combinations.
    /// </summary>
    /// <param name="textureFormatCapabilities">Texture capability metadata to validate.</param>
    static void AssertTextureFormatCapabilities(PlatformTextureFormatCapabilityDefinition textureFormatCapabilities) {
        Assert.NotNull(textureFormatCapabilities);
        Assert.Collection(
            textureFormatCapabilities.SupportedColorFormatIds,
            colorFormatId => Assert.Equal(TextureAssetColorFormat.Rgba4444.ToString(), colorFormatId),
            colorFormatId => Assert.Equal(TextureAssetColorFormat.Indexed8.ToString(), colorFormatId));
        Assert.Equal(
            [TextureAssetAlphaPrecision.A4, TextureAssetAlphaPrecision.A8],
            textureFormatCapabilities.SupportedAlphaPrecisions);
        Assert.Collection(
            textureFormatCapabilities.SupportedCombinations,
            combination => {
                Assert.Equal(TextureAssetColorFormat.Rgba4444.ToString(), combination.ColorFormatId);
                Assert.Equal(TextureAssetAlphaPrecision.A4, combination.AlphaPrecision);
            },
            combination => {
                Assert.Equal(TextureAssetColorFormat.Indexed8.ToString(), combination.ColorFormatId);
                Assert.Equal(TextureAssetAlphaPrecision.A8, combination.AlphaPrecision);
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

        ShaderMaterialAsset materialAsset = EditorAssetTestReader.ReadAsset<ShaderMaterialAsset>(result.CookedMaterialBytes);
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

        ShaderMaterialAsset materialAsset = EditorAssetTestReader.ReadAsset<ShaderMaterialAsset>(result.CookedMaterialBytes);
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

        ShaderMaterialAsset materialAsset = EditorAssetTestReader.ReadAsset<ShaderMaterialAsset>(result.CookedMaterialBytes);
        MaterialConstantBufferAsset baseColorBuffer = Assert.Single(materialAsset.ConstantBuffers, buffer => buffer.Name == "BaseColorBuffer");

        Assert.Equal(1.0f, BitConverter.ToSingle(baseColorBuffer.Data, 0));
        Assert.Equal(64.0f / 255.0f, BitConverter.ToSingle(baseColorBuffer.Data, 4), 5);
        Assert.Equal(64.0f / 255.0f, BitConverter.ToSingle(baseColorBuffer.Data, 8), 5);
        Assert.Equal(128.0f / 255.0f, BitConverter.ToSingle(baseColorBuffer.Data, 12), 5);
    }

    /// <summary>
    /// Verifies the PSP builder can resolve its repository root from the builder assembly location when hosted by another process.
    /// </summary>
    [Fact]
    public void ResolveRepositoryRootPath_uses_builder_assembly_location() {
        string previousRepositoryRoot = Environment.GetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT");
        try {
            Environment.SetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT", null);

            System.Reflection.MethodInfo method = typeof(PspPlatformAssetBuilder).GetMethod("ResolveRepositoryRootPath", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)
                ?? throw new InvalidOperationException("ResolveRepositoryRootPath was not found.");
            string repositoryRootPath = Assert.IsType<string>(method.Invoke(null, null));

            Assert.True(Directory.Exists(Path.Combine(repositoryRootPath, "builder")));
            Assert.True(File.Exists(Path.Combine(repositoryRootPath, "CMakeLists.txt")));
            Assert.True(File.Exists(Path.Combine(repositoryRootPath, "builder", "helengine.psp.builder.csproj")));
        } finally {
            Environment.SetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT", previousRepositoryRoot);
        }
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
                "psp",
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
    /// Verifies the PSP builder accepts the editor-generated unity translation unit when the legacy amalgamated file is absent.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenGivenUnityGeneratedCoreAndCookedArtifacts_produces_psp_game_folder() {
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
        File.WriteAllText(Path.Combine(generatedCoreRoot, "helengine_core_unity.cpp"), "// generated");
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
                "psp",
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
                        "psp-forward",
                        "raw",
                        "psp-scene-v1",
                        PlatformSerializationEndianness.LittleEndian))],
                outputRoot,
                Path.Combine(workingRoot, "builder-work"),
                "psp-debug",
                "psp-forward",
                "default",
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                generatedCoreRoot,
                "psp-game-folder",
                "homebrew-app");

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
            Assert.True(File.Exists(Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "EBOOT.PBP")));
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
    /// Verifies the PSP builder rewrites runtime manifest sources so startup-scene and platform metadata match the current build manifest.
    /// </summary>
    [Fact]
    public async Task BuildAsync_rewrites_runtime_startup_manifest_with_platform_metadata() {
        string workingRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string sourceRoot = Path.Combine(workingRoot, "staging");
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string generatedCoreRuntimeRoot = Path.Combine(generatedCoreRoot, "runtime");
        string repositoryRoot = Path.Combine(workingRoot, "repository");
        string sceneSourcePath = Path.Combine(sourceRoot, "cooked", "scenes", "rendering", "textured_cube_grid.hasset");

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
                "scenes/rendering/textured_cube_grid.helen",
                [
                    new PlatformBuildScene(
                        "scenes/rendering/textured_cube_grid.helen",
                        "Textured Cube Grid",
                        "cooked/scenes/rendering/textured_cube_grid.hasset",
                        [],
                        [new KeyValuePair<string, string>("cooked-relative-path", "cooked/scenes/rendering/textured_cube_grid.hasset")])
                ],
                Array.Empty<PlatformBuildAsset>(),
                [
                    new PlatformBuildArtifact("cooked/scenes/rendering/textured_cube_grid.hasset", "scene:textured-cube-grid", "sha256:scene", "scene", "shared")
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

            await builder.BuildAsync(
                request,
                new RecordingProgressReporter(),
                new RecordingDiagnosticReporter(),
                CancellationToken.None);

            string startupManifestContents = File.ReadAllText(Path.Combine(generatedCoreRuntimeRoot, "runtime_startup_manifest.cpp"));
            Assert.Contains("cooked/scenes/rendering/textured_cube_grid.hasset", startupManifestContents);
            Assert.Contains("static const char kRuntimePlatformName[] = \"psp\";", startupManifestContents);
            Assert.Contains("static const char kRuntimePlatformVersion[] = \"1.0.0\";", startupManifestContents);
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
                "psp",
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

    /// <summary>
    /// Verifies the PSP builder executes one builder-owned texture cook work item into the exact declared runtime output path.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenGivenTexturePlatformCookWorkItem_writesCookedRuntimeTextureToDeclaredOutputPath() {
        string workingRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string sourceRoot = Path.Combine(workingRoot, "staging");
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string generatedCoreRuntimeRoot = Path.Combine(generatedCoreRoot, "runtime");
        string repositoryRoot = Path.Combine(workingRoot, "repository");
        string sceneSourcePath = Path.Combine(sourceRoot, "cooked", "scenes", "rendering", "cube_test.hasset");
        string sourceTexturePath = Path.Combine(workingRoot, "assets", "Textures", "Checker.png");

        Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath)!);
        Directory.CreateDirectory(Path.GetDirectoryName(sourceTexturePath)!);
        Directory.CreateDirectory(generatedCoreRuntimeRoot);
        Directory.CreateDirectory(Path.Combine(repositoryRoot, "src", "platform", "psp"));
        File.WriteAllText(sceneSourcePath, "scene payload");
        File.WriteAllBytes(sourceTexturePath, [1, 2, 3, 4]);
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
                "psp",
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
                    new PlatformBuildArtifact("cooked/scenes/rendering/cube_test.hasset", "scene:cube-test", "sha256:scene", "scene", "shared")
                ],
                Array.Empty<PlatformBuildCodeModule>(),
                Array.Empty<PlatformArtifactPlacement>(),
                new PlatformContainerWritePlan("psp-homebrew", Array.Empty<PlatformContainerArtifact>()),
                [
                    new PlatformCookWorkItem(
                        "psp:texture:cooked/imported/textures/checker",
                        sourceTexturePath,
                        "texture",
                        "psp",
                        "texture",
                        "cooked/imported/textures/checker",
                        "texture:cooked/imported/textures/checker",
                        "sha256:source",
                        "sha256:settings",
                        PspTextureCookSettingsSerializer.Serialize(new TextureAssetProcessorSettings {
                            MaxResolution = 0,
                            ColorFormat = TextureAssetColorFormat.Rgba4444,
                            AlphaPrecision = TextureAssetAlphaPrecision.A4
                        }),
                        [
                            new PlatformCookWorkItemMetadata("source-asset-id", "Textures/Checker")
                        ])
                ]);

            PlatformBuildRequest request = CreateBuildRequest(workingRoot, outputRoot, generatedCoreRoot, manifest);
            TextureAsset expectedTextureAsset = new TextureAsset {
                Width = 1,
                Height = 1,
                ColorFormat = TextureAssetColorFormat.Rgba4444,
                AlphaPrecision = TextureAssetAlphaPrecision.A4,
                Colors = [0xFF, 0x0F]
            };
            FontAsset expectedFontAsset = CreateTestFontAsset();
            FakePspPlatformCookSourceProcessor sourceProcessor = new FakePspPlatformCookSourceProcessor(expectedTextureAsset, expectedFontAsset);
            PspPlatformAssetBuilder builder = new(
                new FakePspNativeBuildExecutor(),
                sourceProcessor);

            PlatformBuildReport report = await builder.BuildAsync(
                request,
                new RecordingProgressReporter(),
                new RecordingDiagnosticReporter(),
                CancellationToken.None);

            Assert.True(report.Succeeded);

            string outputTexturePath = Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "cooked", "imported", "textures", "checker");
            Assert.True(File.Exists(outputTexturePath));

            TextureAsset outputTextureAsset = Assert.IsType<TextureAsset>(AssetSerializer.DeserializeFromBytes(File.ReadAllBytes(outputTexturePath)));
            Assert.Equal("Textures/Checker", outputTextureAsset.Id);
            Assert.Equal(RuntimeAssetIdGenerator.Generate("Textures/Checker"), outputTextureAsset.RuntimeAssetId);
            Assert.Equal(TextureAssetColorFormat.Rgba4444, outputTextureAsset.ColorFormat);
            Assert.Equal(TextureAssetAlphaPrecision.A4, outputTextureAsset.AlphaPrecision);
            Assert.Equal(expectedTextureAsset.Colors, outputTextureAsset.Colors);
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
                    Directory.Delete(workingRoot, true);
                }
            } catch {
            }
        }
    }

    /// <summary>
    /// Verifies the PSP builder executes one builder-owned font-atlas cook work item into one cooked runtime texture payload at the exact declared output path.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenGivenFontAtlasPlatformCookWorkItem_writesCookedRuntimeTextureToDeclaredOutputPath() {
        string workingRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string sourceRoot = Path.Combine(workingRoot, "staging");
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string generatedCoreRuntimeRoot = Path.Combine(generatedCoreRoot, "runtime");
        string repositoryRoot = Path.Combine(workingRoot, "repository");
        string sceneSourcePath = Path.Combine(sourceRoot, "cooked", "scenes", "rendering", "cube_test.hasset");
        string sourceFontPath = Path.Combine(workingRoot, "assets", "Fonts", "Demo.ttf");

        Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath)!);
        Directory.CreateDirectory(Path.GetDirectoryName(sourceFontPath)!);
        Directory.CreateDirectory(generatedCoreRuntimeRoot);
        Directory.CreateDirectory(Path.Combine(repositoryRoot, "src", "platform", "psp"));
        File.WriteAllText(sceneSourcePath, "scene payload");
        File.WriteAllBytes(sourceFontPath, [1, 2, 3, 4]);
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
                "psp",
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
                    new PlatformBuildArtifact("cooked/scenes/rendering/cube_test.hasset", "scene:cube-test", "sha256:scene", "scene", "shared")
                ],
                Array.Empty<PlatformBuildCodeModule>(),
                Array.Empty<PlatformArtifactPlacement>(),
                new PlatformContainerWritePlan("psp-homebrew", Array.Empty<PlatformContainerArtifact>()),
                [
                    new PlatformCookWorkItem(
                        "psp:font-atlas-texture:cooked/fonts/default.ps2tex",
                        sourceFontPath,
                        "font-atlas-texture",
                        "psp",
                        "runtime-texture",
                        "cooked/fonts/default.ps2tex",
                        "texture:cooked/fonts/default.ps2tex",
                        "sha256:source",
                        "sha256:settings",
                        PspTextureCookSettingsSerializer.Serialize(new TextureAssetProcessorSettings {
                            MaxResolution = 128,
                            ColorFormat = TextureAssetColorFormat.Rgba4444,
                            AlphaPrecision = TextureAssetAlphaPrecision.A4
                        }),
                        [
                            new PlatformCookWorkItemMetadata("source-asset-id", "fonts/default.hefont")
                        ])
                ]);

            PlatformBuildRequest request = CreateBuildRequest(workingRoot, outputRoot, generatedCoreRoot, manifest);
            TextureAsset expectedTextureAsset = new TextureAsset {
                Width = 1,
                Height = 1,
                ColorFormat = TextureAssetColorFormat.Rgba4444,
                AlphaPrecision = TextureAssetAlphaPrecision.A4,
                Colors = [0xFF, 0x0F]
            };
            FontAsset expectedFontAsset = CreateTestFontAsset();
            FakePspPlatformCookSourceProcessor sourceProcessor = new FakePspPlatformCookSourceProcessor(expectedTextureAsset, expectedFontAsset);
            PspPlatformAssetBuilder builder = new(
                new FakePspNativeBuildExecutor(),
                sourceProcessor);

            PlatformBuildReport report = await builder.BuildAsync(
                request,
                new RecordingProgressReporter(),
                new RecordingDiagnosticReporter(),
                CancellationToken.None);

            Assert.True(report.Succeeded);

            string outputTexturePath = Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "cooked", "fonts", "default.ps2tex");
            Assert.True(File.Exists(outputTexturePath));
            TextureAsset expectedCookedTextureAsset = sourceProcessor.CookFontAtlasTexture(
                sourceFontPath,
                "fonts/default.hefont",
                new TextureAssetProcessorSettings {
                    MaxResolution = 128,
                    ColorFormat = TextureAssetColorFormat.Rgba4444,
                    AlphaPrecision = TextureAssetAlphaPrecision.A4
                });
            byte[] expectedTextureBytes = global::helengine.files.AssetSerializer.SerializeToBytes(expectedCookedTextureAsset);
            Assert.Equal(expectedTextureBytes, File.ReadAllBytes(outputTexturePath));
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
                    Directory.Delete(workingRoot, true);
                }
            } catch {
            }
        }
    }

    /// <summary>
    /// Verifies the editor-owned scene packager emits one PSP builder-owned texture work item and omits the duplicate generic cooked artifact.
    /// </summary>
    [Fact]
    public void EditorPackager_whenPspOwnsTextureCooking_emitsPlatformCookWorkItem_and_skipsGenericCookedTexture() {
        string workspaceRootPath = Path.Combine(Path.GetTempPath(), "helengine-psp-work-item-tests", Guid.NewGuid().ToString("N"));
        string projectRootPath = workspaceRootPath;
        string buildRootPath = Path.Combine(workspaceRootPath, "Build");
        string sceneId = "Scenes/TexturedScene.helen";
        string materialRelativePath = "Materials/TestMaterial.hasset";
        string textureRelativePath = "Textures/Checker.png";

        Directory.CreateDirectory(Path.Combine(projectRootPath, "assets"));
        Directory.CreateDirectory(Path.Combine(projectRootPath, "cache", "shader-cache"));
        Directory.CreateDirectory(buildRootPath);

        try {
            string textureAssetId = WriteSourceTextureAssetAndReturnAssetId(projectRootPath, textureRelativePath, "psp");
            WritePspMaterialAsset(projectRootPath, materialRelativePath, textureAssetId);
            WriteSceneAssetWithMaterial(projectRootPath, sceneId, materialRelativePath);
            SeedBuiltInStandardShaderAsset(ShaderCompileTarget.DirectX11);
            PspPlatformAssetBuilder builder = new(new FakePspNativeBuildExecutor(), new FakePspPlatformCookSourceProcessor(
                new TextureAsset {
                    Width = 1,
                    Height = 1,
                    Colors = [255, 255, 255, 255]
                },
                CreateTestFontAsset()));

            EditorPlatformBuildScenePackager packager = new(
                projectRootPath,
                [
                    new TextureImporterRegistration("test-texture", new PspBuilderTestTextureImporter(), [".png"])
                ],
                builder.Definition,
                CreateTestFontAsset(),
                builder,
                "debug",
                "psp-forward");

            EditorPlatformBuildScenePackagerResult result = packager.Package([sceneId], buildRootPath);

            PlatformCookWorkItem workItem = Assert.Single(result.PlatformCookWorkItems);
            Assert.Equal("texture", workItem.SourceAssetKind);
            Assert.Equal($"cooked/imported/{textureAssetId}", workItem.OutputRelativePath);
            Assert.Equal("psp", workItem.TargetPlatformId);
            Assert.False(File.Exists(Path.Combine(buildRootPath, "cooked", "imported", textureAssetId)));
        } finally {
            try {
                if (Directory.Exists(workspaceRootPath)) {
                    Directory.Delete(workspaceRootPath, true);
                }
            } catch {
            }
        }
    }

    /// <summary>
    /// Seeds the built-in shader cache with one precompiled standard shader so scene-packager tests do not require runtime shader compilation.
    /// </summary>
    /// <param name="target">Shader compile target whose cache entry should be seeded.</param>
    static void SeedBuiltInStandardShaderAsset(ShaderCompileTarget target) {
        string shaderPath = EditorBuiltInShaderAssetLibrary.ResolveShaderPath("ForwardStandardShader.hlsl");
        string cacheKey = string.Concat(target.ToString(), "|", shaderPath);
        ShaderAsset shaderAsset = new ShaderAsset {
            Id = "ForwardStandardShader",
            Name = "ForwardStandardShader",
            TargetName = ShaderTargetNames.GetTargetName(target),
            Programs = [
                new ShaderProgramAsset {
                    Name = "ForwardStandardShader.vs",
                    Stage = ShaderStage.Vertex,
                    EntryPoint = "VS",
                    Bindings = Array.Empty<ShaderBindingAsset>(),
                    Inputs = Array.Empty<ShaderVertexElementAsset>(),
                    Outputs = Array.Empty<ShaderVertexElementAsset>(),
                    Variants = [
                        new ShaderVariantAsset {
                            Name = "default",
                            Defines = Array.Empty<string>()
                        }
                    ]
                }
            ],
            Binaries = [
                new ShaderBinaryAsset {
                    ProgramName = "ForwardStandardShader.vs",
                    Stage = ShaderStage.Vertex,
                    TargetName = ShaderTargetNames.GetTargetName(target),
                    Variant = "default",
                    Bytecode = [1, 2, 3, 4]
                }
            ]
        };

        FieldInfo cacheField = typeof(EditorBuiltInShaderAssetLibrary).GetField("ShaderAssetsByKey", BindingFlags.Static | BindingFlags.NonPublic) ?? throw new InvalidOperationException("Built-in shader cache field was not found.");
        Dictionary<string, ShaderAsset> cache = Assert.IsType<Dictionary<string, ShaderAsset>>(cacheField.GetValue(null));
        cache[cacheKey] = shaderAsset;
    }

    /// <summary>
    /// Creates one fully resolved PSP build request used by builder execution tests.
    /// </summary>
    /// <param name="workingRoot">Temporary working root used by the build request.</param>
    /// <param name="outputRoot">Final output root used by the build request.</param>
    /// <param name="generatedCoreRoot">Generated core source root used by the build request.</param>
    /// <param name="manifest">Resolved build manifest consumed by the PSP builder.</param>
    /// <returns>Fully resolved PSP build request.</returns>
    static PlatformBuildRequest CreateBuildRequest(string workingRoot, string outputRoot, string generatedCoreRoot, PlatformBuildManifest manifest) {
        return new PlatformBuildRequest(
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
    }

    /// <summary>
    /// Creates one deterministic packaged font asset for PSP builder tests.
    /// </summary>
    /// <returns>Deterministic packaged font asset.</returns>
    static FontAsset CreateTestFontAsset() {
        FontAsset fontAsset = new FontAsset(
            new FontInfo("TestFont", 16, 4f),
            new FakeRuntimeTexture {
                Width = 1,
                Height = 1
            },
            new Dictionary<char, FontChar> {
                ['A'] = new FontChar {
                    SourceRect = new float4(0f, 0f, 1f, 1f),
                    OffsetY = 0f,
                    AdvanceWidth = 8f,
                    BearingX = 0f,
                    BearingY = 0f
                }
            },
            16f,
            1,
            1);
        fontAsset.SourceTextureAsset = new TextureAsset {
            Id = "fonts/default.hefont#atlas",
            RuntimeAssetId = RuntimeAssetIdGenerator.Generate("fonts/default.hefont#atlas"),
            Width = 1,
            Height = 1,
            ColorFormat = TextureAssetColorFormat.Rgba4444,
            AlphaPrecision = TextureAssetAlphaPrecision.A4,
            Colors = [0xFF, 0x0F]
        };
        return fontAsset;
    }

    /// <summary>
    /// Writes one source texture file and resolves its imported asset id for PSP editor-owned cook verification.
    /// </summary>
    /// <param name="projectRootPath">Temporary editor project root path.</param>
    /// <param name="textureRelativePath">Project-relative source texture path to create.</param>
    /// <param name="platformId">Platform identifier used to resolve import settings.</param>
    /// <returns>Resolved imported texture asset id.</returns>
    static string WriteSourceTextureAssetAndReturnAssetId(string projectRootPath, string textureRelativePath, string platformId) {
        string textureSourcePath = Path.Combine(projectRootPath, "assets", textureRelativePath.Replace('/', Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(textureSourcePath)!);
        File.WriteAllBytes(textureSourcePath, [1, 2, 3, 4]);

        ContentManager contentManager = new ContentManager(projectRootPath);
        AssetImportManager assetImportManager = new AssetImportManager(projectRootPath, contentManager);
        assetImportManager.CurrentPlatformId = platformId;
        assetImportManager.RegisterTextureImporter(new TextureImporterRegistration("test-texture", new PspBuilderTestTextureImporter(), [".png"]));

        TextureAssetImportSettings settings;
        Assert.True(assetImportManager.TryLoadOrCreateTextureImportSettings(textureSourcePath, out settings));
        Assert.NotNull(settings);
        Assert.NotNull(settings.Importer);
        Assert.False(string.IsNullOrWhiteSpace(settings.Importer.AssetId));
        return settings.Importer.AssetId;
    }

    /// <summary>
    /// Writes one authored PSP material settings document that references the supplied imported diffuse texture id.
    /// </summary>
    /// <param name="projectRootPath">Temporary editor project root path.</param>
    /// <param name="materialRelativePath">Project-relative material path to write.</param>
    /// <param name="diffuseTextureAssetId">Imported diffuse texture asset id referenced by the material.</param>
    static void WritePspMaterialAsset(string projectRootPath, string materialRelativePath, string diffuseTextureAssetId) {
        string materialPath = Path.Combine(projectRootPath, "assets", materialRelativePath.Replace('/', Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(materialPath)!);

        MaterialAssetImportSettings settings = new MaterialAssetImportSettings {
            Importer = new AssetImporterSettings {
                ImporterId = "helengine.material",
                SourceChecksum = string.Empty,
                AssetId = materialRelativePath
            },
            Processor = new MaterialAssetProcessorPlatformSettings()
        };
        settings.Processor.Platforms["psp"] = new MaterialAssetProcessorSettings {
            SchemaId = "standard-shader",
            FieldValues = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) {
                ["use-custom-shader"] = "false",
                ["shader-asset-id"] = "engine:material:standard",
                ["vertex-program"] = "ForwardStandard.vs",
                ["pixel-program"] = "ForwardStandard.ps",
                ["variant"] = "Mesh",
                ["texture-id"] = diffuseTextureAssetId,
                ["casts-shadow"] = "true",
                ["receives-shadow"] = "true",
                ["base-color"] = "#FFFFFFFF"
            }
        };

        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        settingsService.Save(materialPath, settings);
    }

    /// <summary>
    /// Writes one serialized scene asset whose mesh component references the supplied file-backed material.
    /// </summary>
    /// <param name="projectRootPath">Temporary editor project root path.</param>
    /// <param name="sceneId">Stable scene id to write.</param>
    /// <param name="materialRelativePath">Project-relative material path referenced by the mesh component.</param>
    static void WriteSceneAssetWithMaterial(string projectRootPath, string sceneId, string materialRelativePath) {
        string scenePath = Path.Combine(projectRootPath, "assets", sceneId.Replace('/', Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(scenePath)!);

        SceneAsset sceneAsset = new SceneAsset {
            Id = sceneId,
            RootEntities = [
                new SceneEntityAsset {
                    Id = 1u,
                    Name = "MeshRoot",
                    LocalPosition = float3.Zero,
                    LocalScale = float3.One,
                    LocalOrientation = float4.Identity,
                    Components = [
                        new SceneComponentAssetRecord {
                            ComponentTypeId = "helengine.MeshComponent",
                            ComponentIndex = 0,
                            Payload = WriteMeshComponentPayload(materialRelativePath)
                        }
                    ],
                    Children = Array.Empty<SceneEntityAsset>()
                }
            ]
        };

        using FileStream stream = new FileStream(scenePath, FileMode.Create, FileAccess.Write, FileShare.None);
        global::helengine.editor.AssetSerializer.Serialize(stream, sceneAsset);
    }

    /// <summary>
    /// Writes one mesh-component payload that points at the supplied file-backed material path.
    /// </summary>
    /// <param name="materialRelativePath">Project-relative material path to encode.</param>
    /// <returns>Serialized mesh-component payload.</returns>
    static byte[] WriteMeshComponentPayload(string materialRelativePath) {
        MeshComponent meshComponent = new MeshComponent();
        meshComponent.SetMaterials(new RuntimeMaterial[] {
            new TestRuntimeMaterial()
        });
        EntityComponentSaveState saveState = new EntityComponentSaveState();
        saveState.SetAssetReference(
            "Materials[0]",
            global::helengine.SceneAssetReferenceFactory.CreateFileSystemMaterial(materialRelativePath));

        ComponentPersistenceRegistry persistenceRegistry = new ComponentPersistenceRegistry();
        SceneComponentAssetRecord record = persistenceRegistry.GetDescriptor(meshComponent).SerializeComponent(meshComponent, 0, saveState);
        return record.Payload;
    }
}
