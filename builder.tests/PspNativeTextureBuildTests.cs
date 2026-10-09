using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.editor;

namespace helengine.psp.builder.tests;

/// <summary>Verifies real GIM and atlas processing survives builder staging before the native build handoff.</summary>
[Collection(nameof(PspBuildProcessStateCollection))]
public sealed class PspNativeTextureBuildTests {
    /// <summary>Executes actual source decoding and native cooking, replacing only the final SDK invocation.</summary>
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public async Task BuildAsync_stages_real_native_texture_and_font_atlas(bool atlas) {
        string root = Path.Combine(Environment.GetEnvironmentVariable("HELENGINE_PSP_TEXTURE_TEST_OUTPUT") ?? AppContext.BaseDirectory, "build-" + Guid.NewGuid().ToString("N"));
        string generated = Path.Combine(root, "generated");
        string repository = Path.Combine(root, "repository");
        string staged = Path.Combine(root, "staging");
        Directory.CreateDirectory(Path.Combine(generated, "runtime"));
        Directory.CreateDirectory(Path.Combine(repository, "src", "platform", "psp"));
        Directory.CreateDirectory(Path.Combine(staged, "cooked"));
        File.WriteAllText(Path.Combine(repository, "Makefile"), "# fixture");
        File.WriteAllText(Path.Combine(repository, "src", "platform", "psp", "PspBootHost.cpp"), "// fixture");
        File.WriteAllText(Path.Combine(generated, "helengine_core_amalgamated.cpp"), "// fixture");
        File.WriteAllText(Path.Combine(generated, "helcpp_config.hpp"), "#pragma once");
        File.WriteAllText(Path.Combine(generated, "runtime", "runtime_startup_manifest.cpp"), "// fixture");
        File.WriteAllText(Path.Combine(generated, "runtime", "runtime_scene_catalog_manifest.cpp"), "// fixture");
        File.WriteAllText(Path.Combine(staged, "cooked", "scene.hasset"), "scene fixture");
        using TextureAsset source = new TextureAsset { Width = 32, Height = 8, ColorFormat = TextureAssetColorFormat.Rgba32,
            AlphaPrecision = TextureAssetAlphaPrecision.A8, Colors = Enumerable.Range(0, 32 * 8 * 4).Select(value => (byte)value).ToArray() };
        string sourcePath = Path.Combine(root, atlas ? "atlas.hasset" : "source.gim");
        if (atlas) File.WriteAllBytes(sourcePath, global::helengine.files.AssetSerializer.SerializeToBytes(source));
        else {
            using TextureAsset container = PspTextureCodec.Encode(source, "Psp.Linear.Rgba8888", TextureAssetAlphaPrecision.A8);
            File.WriteAllBytes(sourcePath, PspGimTestFile.Create(container));
        }
        string format = atlas ? "Psp.Swizzled.T8.ClutRgba8888" : "Psp.Linear.Dxt5";
        string destination = atlas ? "cooked/atlas.hetex" : "cooked/image.hetex";
        PlatformBuildManifest manifest = new PlatformBuildManifest(3, "fixture", "1", "1", "psp", "1", "scene.helen",
            new[] { new PlatformBuildScene("scene.helen", "Fixture", "cooked/scene.hasset", Array.Empty<PlatformBuildPayloadReference>(), new[] { new KeyValuePair<string, string>("cooked-relative-path", "cooked/scene.hasset") }) },
            Array.Empty<PlatformBuildAsset>(), new[] { new PlatformBuildArtifact("cooked/scene.hasset", "scene", "sha256:scene", "scene", "shared") },
            Array.Empty<PlatformBuildCodeModule>(), Array.Empty<PlatformArtifactPlacement>(), new PlatformContainerWritePlan("psp-homebrew", Array.Empty<PlatformContainerArtifact>()),
            new[] { new PlatformCookWorkItem("native-texture", sourcePath, atlas ? "font-atlas-texture" : "texture", "psp", "runtime-texture", destination, "texture", "sha256:source", "sha256:settings",
                PspTextureCookSettingsSerializer.Serialize(new TextureAssetProcessorSettings { ColorFormatId = format, AlphaPrecision = TextureAssetAlphaPrecision.A8, MaxResolution = 512 }),
                new[] { new PlatformCookWorkItemMetadata("source-asset-id", "texture:fixture") }) }, PlatformBuildRuntimeFeatureManifest.Empty);
        PlatformBuildRequest request = new PlatformBuildRequest(manifest, new[] { new PlatformBuildTargetVariant("psp-debug", "psp", "psp", "default") },
            new[] { new PlatformCookProfile("default", "Default", new PlatformCookProfileCapabilities("psp", "raw", "rgba", "psp-scene-v1", PlatformSerializationEndianness.LittleEndian)) },
            Path.Combine(root, "out"), Path.Combine(root, "work"), selectedBuildProfileId: "debug", selectedGraphicsProfileId: "psp-forward", selectedCodegenProfileId: "default",
            selectedBuildOptionValues: new Dictionary<string, string>(), selectedGraphicsOptionValues: new Dictionary<string, string>(), selectedCodegenOptionValues: new Dictionary<string, string>(),
            generatedCoreCppRootPath: generated, selectedMediaProfileId: "psp-game-folder", selectedStorageProfileId: "homebrew-app");
        string previousDirectory = Directory.GetCurrentDirectory();
        string previousRepository = Environment.GetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT");
        try {
            Directory.SetCurrentDirectory(staged); Environment.SetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT", repository);
            FakePspNativeBuildExecutor native = new FakePspNativeBuildExecutor();
            PspPlatformAssetBuilder builder = new PspPlatformAssetBuilder(native, new PspPlatformCookSourceProcessor());
            var report = await builder.BuildAsync(request, new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None);
            Assert.True(report.Succeeded, string.Join("\n", report.Diagnostics.Select(item => item.ToString())));
            Assert.NotNull(native.LastWorkspace);
            string output = Path.Combine(root, "out", "PSP", "GAME", "HELENGINE", destination.Replace('/', Path.DirectorySeparatorChar));
            using TextureAsset result = Assert.IsType<TextureAsset>(global::helengine.files.AssetSerializer.DeserializeFromBytes(File.ReadAllBytes(output)));
            Assert.Equal(atlas ? "texture:fixture#atlas" : "texture:fixture", result.Id);
            PspTextureLayout layout = PspTextureCodec.ReadLayout(result);
            Assert.Equal(atlas ? 5 : 10, layout.Format.Code);
            Assert.Equal(atlas, layout.Format.Swizzled);
        } finally {
            Directory.SetCurrentDirectory(previousDirectory); Environment.SetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT", previousRepository);
        }
    }
}
