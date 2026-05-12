using helengine.editor;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the authored city textured cube-grid scene stays compatible with the PSP textured-material contract.
/// </summary>
public sealed class CityTexturedCubeGridSceneTests {
    /// <summary>
    /// Absolute city project root used by the shared rendering-scene regression.
    /// </summary>
    const string CityProjectRootPath = @"C:\dev\helprojs\city";

    /// <summary>
    /// Relative authored material path for the first textured cube material.
    /// </summary>
    const string Cube00MaterialRelativePath = @"assets\Materials\rendering\textured_cube_grid\Cube00.helmat";

    /// <summary>
    /// Ensures the generated city textured cube material sidecar includes the PSP diffuse texture binding.
    /// </summary>
    [Fact]
    public void TexturedCubeGridMaterialSettings_include_psp_diffuse_texture_binding() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        string materialPath = Path.Combine(CityProjectRootPath, Cube00MaterialRelativePath);

        Assert.True(settingsService.TryLoad(materialPath, out AssetImportSettings settings));
        Assert.NotNull(settings);
        Assert.True(settings.Processor.Platforms.ContainsKey("psp"));
        Assert.Equal("standard-shader", settings.Processor.Platforms["psp"].Material.SchemaId);
        Assert.True(settings.Processor.Platforms["psp"].Material.FieldValues.ContainsKey("texture-id"));
        Assert.False(string.IsNullOrWhiteSpace(settings.Processor.Platforms["psp"].Material.FieldValues["texture-id"]));
    }

    /// <summary>
    /// Ensures the PSP material cook path preserves the authored diffuse texture asset id for the textured cube scene.
    /// </summary>
    [Fact]
    public void TexturedCubeGridMaterialCook_preserves_psp_diffuse_texture_asset_id() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        string materialPath = Path.Combine(CityProjectRootPath, Cube00MaterialRelativePath);

        Assert.True(settingsService.TryLoad(materialPath, out AssetImportSettings settings));
        Assert.NotNull(settings);
        Assert.True(settings.Processor.Platforms.ContainsKey("psp"));

        MaterialAsset sourceMaterialAsset = ReadTexturedCubeGridMaterialAsset();
        Assert.False(string.IsNullOrWhiteSpace(sourceMaterialAsset.DiffuseTextureAssetId));

        AssetPlatformProcessorSettings platformSettings = settings.Processor.Platforms["psp"];
        Dictionary<string, string> fieldValues = new Dictionary<string, string>(platformSettings.Material.FieldValues) {
            ["shader-asset-id"] = sourceMaterialAsset.ShaderAssetId,
            ["vertex-program"] = sourceMaterialAsset.VertexProgram,
            ["pixel-program"] = sourceMaterialAsset.PixelProgram,
            ["variant"] = sourceMaterialAsset.Variant
        };
        PspPlatformAssetBuilder builder = new PspPlatformAssetBuilder();
        PlatformMaterialCookResult cookResult = builder.CookMaterial(new PlatformMaterialCookRequest(
            Cube00MaterialRelativePath.Replace('\\', '/'),
            Cube00MaterialRelativePath.Replace('\\', '/'),
            "psp",
            "debug",
            "psp-forward",
            platformSettings.Material.SchemaId,
            fieldValues));

        MaterialAsset materialAsset = Assert.IsType<MaterialAsset>(AssetSerializer.DeserializeFromBytes(cookResult.CookedMaterialBytes));

        Assert.Equal(sourceMaterialAsset.DiffuseTextureAssetId, materialAsset.DiffuseTextureAssetId);
    }

    /// <summary>
    /// Reads the authored first textured-cube material asset from disk.
    /// </summary>
    /// <returns>Deserialized authored material asset.</returns>
    MaterialAsset ReadTexturedCubeGridMaterialAsset() {
        string materialPath = Path.Combine(CityProjectRootPath, Cube00MaterialRelativePath);
        Assert.True(File.Exists(materialPath));

        using FileStream stream = File.OpenRead(materialPath);
        return Assert.IsType<MaterialAsset>(EditorAssetBinarySerializer.Deserialize(stream));
    }
}
