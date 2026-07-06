using helengine.editor;

namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the shared material-settings normalization path preserves the authored PSP sphere-stack texture binding before packaging.
/// </summary>
public sealed class CitySphereStackMaterialLoadOrCreateTests {
    /// <summary>
    /// Absolute authored sphere-stack material path used by the shared physics-scene regression.
    /// </summary>
    const string AuthoredMaterialPath = @"C:\dev\helprojs\city\assets\Materials\physics\PhysicsDemoSphereStackBlue.hasset";

    /// <summary>
    /// Absolute authored sphere-stack material paths used by the mixed-stack sphere variants.
    /// </summary>
    static readonly string[] MixedStackSphereMaterialPaths = new[] {
        @"C:\dev\helprojs\city\assets\Materials\physics\PhysicsDemoSphereStackGreen.hasset",
        @"C:\dev\helprojs\city\assets\Materials\physics\PhysicsDemoSphereStackYellow.hasset",
        @"C:\dev\helprojs\city\assets\Materials\physics\PhysicsDemoSphereStackRed.hasset",
        @"C:\dev\helprojs\city\assets\Materials\physics\PhysicsDemoSphereStackPurple.hasset"
    };

    /// <summary>
    /// Ensures the material-settings normalization path used by scene packaging retains the authored PSP texture-id field.
    /// </summary>
    [Fact]
    public void City_sphere_stack_material_load_or_create_preserves_psp_texture_id() {
        MaterialAssetSettingsService service = new MaterialAssetSettingsService();
        PspPlatformAssetBuilder builder = new();

        Assert.True(File.Exists(AuthoredMaterialPath));

        ShaderMaterialAsset materialAsset = service.LoadMaterialAsset(AuthoredMaterialPath, "psp");
        MaterialAssetImportSettings settings = service.LoadOrCreate(
            AuthoredMaterialPath,
            materialAsset,
            ["psp"],
            _ => EditorPlatformBuildSelectionModel.From(builder.Definition));

        Assert.NotNull(settings);
        Assert.True(settings.Processor.Platforms.TryGetValue("psp", out MaterialAssetProcessorSettings platformSettings));
        Assert.NotNull(platformSettings);
        Assert.True(platformSettings.FieldValues.TryGetValue("texture-id", out string textureAssetId));
        Assert.False(string.IsNullOrWhiteSpace(textureAssetId));
    }

    /// <summary>
    /// Ensures each mixed-stack sphere material preserves the authored PSP texture-id field through the shared normalization path used by packaging.
    /// </summary>
    [Fact]
    public void City_mixed_stack_sphere_materials_load_or_create_preserve_psp_texture_id() {
        MaterialAssetSettingsService service = new MaterialAssetSettingsService();
        PspPlatformAssetBuilder builder = new();

        for (int index = 0; index < MixedStackSphereMaterialPaths.Length; index++) {
            string materialPath = MixedStackSphereMaterialPaths[index];
            Assert.True(File.Exists(materialPath));

            ShaderMaterialAsset materialAsset = service.LoadMaterialAsset(materialPath, "psp");
            MaterialAssetImportSettings settings = service.LoadOrCreate(
                materialPath,
                materialAsset,
                ["psp"],
                _ => EditorPlatformBuildSelectionModel.From(builder.Definition));

            Assert.NotNull(settings);
            Assert.True(settings.Processor.Platforms.TryGetValue("psp", out MaterialAssetProcessorSettings platformSettings));
            Assert.NotNull(platformSettings);
            Assert.True(platformSettings.FieldValues.TryGetValue("texture-id", out string textureAssetId));
            Assert.False(string.IsNullOrWhiteSpace(textureAssetId));
        }
    }
}
