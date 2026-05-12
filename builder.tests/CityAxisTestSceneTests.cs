using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using helengine.editor;

namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the authored city axis-test scene stays compatible with the PSP solid-color material contract.
/// </summary>
public sealed class CityAxisTestSceneTests {
    /// <summary>
    /// Absolute city project root used by the shared rendering-scene regression.
    /// </summary>
    const string CityProjectRootPath = @"C:\dev\helprojs\city";

    /// <summary>
    /// Relative authored material path for the first axis-test material.
    /// </summary>
    const string AxisXMaterialRelativePath = @"assets\materials\rendering\axis_test\X.helmat";

    /// <summary>
    /// Ensures the generated city axis-test material sidecar includes PSP material settings with the authored base color.
    /// </summary>
    [Fact]
    public void AxisTestMaterialSettings_include_psp_base_color() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        string materialPath = Path.Combine(CityProjectRootPath, AxisXMaterialRelativePath);

        Assert.True(settingsService.TryLoad(materialPath, out AssetImportSettings settings));
        Assert.NotNull(settings);
        Assert.True(settings.Processor.Platforms.ContainsKey("psp"));
        Assert.Equal("standard-shader", settings.Processor.Platforms["psp"].Material.SchemaId);
        Assert.Equal("#FF4040FF", settings.Processor.Platforms["psp"].Material.FieldValues["base-color"]);
    }

    /// <summary>
    /// Ensures the generated city axis-test material cooks into a PSP base-color buffer that preserves the authored color.
    /// </summary>
    [Fact]
    public void AxisTestMaterialCook_preserves_psp_base_color() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        string materialPath = Path.Combine(CityProjectRootPath, AxisXMaterialRelativePath);

        Assert.True(settingsService.TryLoad(materialPath, out AssetImportSettings settings));
        Assert.NotNull(settings);
        Assert.True(settings.Processor.Platforms.ContainsKey("psp"));

        MaterialAsset sourceMaterialAsset = ReadAxisTestMaterialAsset();
        AssetPlatformProcessorSettings platformSettings = settings.Processor.Platforms["psp"];
        Dictionary<string, string> fieldValues = new Dictionary<string, string>(platformSettings.Material.FieldValues) {
            ["shader-asset-id"] = sourceMaterialAsset.ShaderAssetId,
            ["vertex-program"] = sourceMaterialAsset.VertexProgram,
            ["pixel-program"] = sourceMaterialAsset.PixelProgram,
            ["variant"] = sourceMaterialAsset.Variant
        };
        PspPlatformAssetBuilder builder = new PspPlatformAssetBuilder();
        PlatformMaterialCookResult cookResult = builder.CookMaterial(new PlatformMaterialCookRequest(
            AxisXMaterialRelativePath.Replace('\\', '/'),
            AxisXMaterialRelativePath.Replace('\\', '/'),
            "psp",
            "debug",
            "psp-forward",
            platformSettings.Material.SchemaId,
            fieldValues));

        MaterialAsset materialAsset = Assert.IsType<MaterialAsset>(AssetSerializer.DeserializeFromBytes(cookResult.CookedMaterialBytes));
        MaterialConstantBufferAsset baseColorBuffer = Assert.Single(
            materialAsset.ConstantBuffers,
            buffer => buffer.Name == "BaseColorBuffer");

        Assert.Equal(1.0f, BitConverter.ToSingle(baseColorBuffer.Data, 0));
        Assert.Equal(64.0f / 255.0f, BitConverter.ToSingle(baseColorBuffer.Data, 4), 5);
        Assert.Equal(64.0f / 255.0f, BitConverter.ToSingle(baseColorBuffer.Data, 8), 5);
        Assert.Equal(1.0f, BitConverter.ToSingle(baseColorBuffer.Data, 12));
    }

    /// <summary>
    /// Reads the authored first axis-test material asset from disk.
    /// </summary>
    /// <returns>Deserialized authored material asset.</returns>
    MaterialAsset ReadAxisTestMaterialAsset() {
        string materialPath = Path.Combine(CityProjectRootPath, AxisXMaterialRelativePath);
        Assert.True(File.Exists(materialPath));

        using FileStream stream = File.OpenRead(materialPath);
        return Assert.IsType<MaterialAsset>(EditorAssetBinarySerializer.Deserialize(stream));
    }
}
