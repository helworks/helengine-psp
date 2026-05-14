using helengine.baseplatform.Manifest;

namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the PSP runtime native manifest writer embeds startup-scene and platform metadata into generated-core runtime source.
/// </summary>
public sealed class PspRuntimeNativeManifestWriterTests {
    /// <summary>
    /// Ensures the PSP writer emits startup-scene, platform-name, and platform-version functions into the generated startup manifest.
    /// </summary>
    [Fact]
    public void Write_embeds_startup_scene_and_platform_metadata() {
        string workingRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string runtimeRoot = Path.Combine(generatedCoreRoot, "runtime");
        Directory.CreateDirectory(runtimeRoot);

        try {
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

            new PspRuntimeNativeManifestWriter().Write(generatedCoreRoot, manifest);

            string headerContents = File.ReadAllText(Path.Combine(runtimeRoot, "runtime_startup_manifest.hpp"));
            string sourceContents = File.ReadAllText(Path.Combine(runtimeRoot, "runtime_startup_manifest.cpp"));

            Assert.Contains("const char* he_get_runtime_startup_scene_relative_path();", headerContents);
            Assert.Contains("const char* he_get_runtime_platform_name();", headerContents);
            Assert.Contains("const char* he_get_runtime_platform_version();", headerContents);
            Assert.Contains("cooked/scenes/rendering/textured_cube_grid.hasset", sourceContents);
            Assert.Contains("static const char kRuntimePlatformName[] = \"psp\";", sourceContents);
            Assert.Contains("static const char kRuntimePlatformVersion[] = \"1.0.0\";", sourceContents);
        } finally {
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, true);
            }
        }
    }
}
