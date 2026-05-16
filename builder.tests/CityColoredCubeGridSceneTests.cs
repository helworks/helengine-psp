using helengine.editor;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the authored city colored cube-grid scene stays compatible with PSP runtime directional-shadow motion deserialization.
/// </summary>
public sealed class CityColoredCubeGridSceneTests {
    /// <summary>
    /// Absolute city project root used by the shared rendering-scene regression.
    /// </summary>
    const string CityProjectRootPath = @"C:\dev\helprojs\city";

    /// <summary>
    /// Relative authored scene path for the colored cube-grid showcase.
    /// </summary>
    const string ColoredCubeGridSceneRelativePath = @"assets\scenes\rendering\colored_cube_grid.helen";

    /// <summary>
    /// Stable serialized type id for the demo-disc return-to-menu runtime component.
    /// </summary>
    const string ReturnToMenuComponentTypeId = "city.menu.DemoDiscReturnToMenuComponent, gameplay";

    /// <summary>
    /// Stable serialized type id for the authored colored-cube gameplay tower-spin component.
    /// </summary>
    const string GameplayTowerSpinComponentTypeId = "gameplay.rendering.DirectionalShadowTowerSpinComponent, gameplay";

    /// <summary>
    /// Relative authored material paths for the sixteen colored cube materials.
    /// </summary>
    static readonly string[] CubeMaterialRelativePaths = [
        @"assets\Materials\rendering\colored_cube_grid\Cube00.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube01.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube02.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube03.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube04.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube05.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube06.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube07.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube08.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube09.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube10.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube11.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube12.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube13.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube14.hasset",
        @"assets\Materials\rendering\colored_cube_grid\Cube15.hasset"
    ];

    /// <summary>
    /// Authored PSP base colors expected for the sixteen cube materials.
    /// </summary>
    static readonly string[] ExpectedCubeMaterialColors = [
        "#FF4040FF", "#FF8040FF", "#FFC040FF", "#FFFF40FF",
        "#C0FF40FF", "#80FF40FF", "#40FF40FF", "#40FF80FF",
        "#40FFC0FF", "#40FFFFFF", "#40C0FFFF", "#4080FFFF",
        "#4040FFFF", "#8040FFFF", "#C040FFFF", "#FF40FFFF"
    ];

    /// <summary>
    /// Ensures the authored colored cube-grid scene encodes the gameplay tower-spin payload using the runtime binary contract.
    /// </summary>
    [Fact]
    public void ColoredCubeGridScene_writes_gameplay_tower_spin_payload() {
        SceneAsset sceneAsset = ReadColoredCubeGridSceneAsset();
        List<SceneComponentAssetRecord> towerSpinRecords = new List<SceneComponentAssetRecord>();

        CollectComponentsByTypeId(
            sceneAsset.RootEntities ?? Array.Empty<SceneEntityAsset>(),
            GameplayTowerSpinComponentTypeId,
            towerSpinRecords);

        Assert.Equal(16, towerSpinRecords.Count);
        for (int index = 0; index < towerSpinRecords.Count; index++) {
            DirectionalShadowTowerSpinComponent component = ReadTowerSpinComponent(towerSpinRecords[index]);

            Assert.Equal(0f, component.BaseYawRadians, 5);
            Assert.Equal((float)(Math.PI / 2.0), component.AngularSpeedRadians, 5);
        }
    }

    /// <summary>
    /// Ensures the authored colored cube-grid scene writes the demo-disc return-to-menu component using the tagged editor scripted-component payload contract.
    /// </summary>
    [Fact]
    public void ColoredCubeGridScene_writes_return_to_menu_payload() {
        SceneAsset sceneAsset = ReadColoredCubeGridSceneAsset();
        List<SceneComponentAssetRecord> returnToMenuRecords = new List<SceneComponentAssetRecord>();

        CollectComponentsByTypeId(
            sceneAsset.RootEntities ?? Array.Empty<SceneEntityAsset>(),
            ReturnToMenuComponentTypeId,
            returnToMenuRecords);

        SceneComponentAssetRecord returnToMenuRecord = Assert.Single(returnToMenuRecords);
        EditorTaggedSceneComponentFieldReader reader = new EditorTaggedSceneComponentFieldReader(returnToMenuRecord.Payload ?? Array.Empty<byte>());

        Assert.True(reader.TryGetFieldReader("UpdateOrder", out EngineBinaryReader updateOrderReader));
        using (updateOrderReader) {
            Assert.Equal((byte)0, updateOrderReader.ReadByte());
        }
    }

    /// <summary>
    /// Ensures all sixteen colored-cube PSP material settings preserve their authored base colors.
    /// </summary>
    [Fact]
    public void ColoredCubeGridMaterialSettings_preserve_all_psp_base_colors() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();

        for (int cubeIndex = 0; cubeIndex < CubeMaterialRelativePaths.Length; cubeIndex++) {
            string materialPath = Path.Combine(CityProjectRootPath, CubeMaterialRelativePaths[cubeIndex]);

            Assert.True(settingsService.TryLoadPlatformSettings(materialPath, "psp", out MaterialAssetProcessorSettings platformSettings));
            Assert.NotNull(platformSettings);
            Assert.Equal("standard-shader", platformSettings.SchemaId);
            Assert.Equal(
                ExpectedCubeMaterialColors[cubeIndex],
                platformSettings.FieldValues["base-color"]);
        }
    }

    /// <summary>
    /// Ensures all sixteen colored cube materials cook into PSP base-color buffers that preserve the authored colors.
    /// </summary>
    [Fact]
    public void ColoredCubeGridMaterialCook_preserves_all_psp_base_colors() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        PspPlatformAssetBuilder builder = new PspPlatformAssetBuilder();

        for (int cubeIndex = 0; cubeIndex < CubeMaterialRelativePaths.Length; cubeIndex++) {
            string materialRelativePath = CubeMaterialRelativePaths[cubeIndex];
            string materialPath = Path.Combine(CityProjectRootPath, materialRelativePath);

            Assert.True(settingsService.TryLoadPlatformSettings(materialPath, "psp", out MaterialAssetProcessorSettings platformSettings));
            Assert.NotNull(platformSettings);

            MaterialAsset sourceMaterialAsset = LoadMaterialAsset(materialPath);
            Dictionary<string, string> fieldValues = new Dictionary<string, string>(platformSettings.FieldValues) {
                ["shader-asset-id"] = sourceMaterialAsset.ShaderAssetId,
                ["vertex-program"] = sourceMaterialAsset.VertexProgram,
                ["pixel-program"] = sourceMaterialAsset.PixelProgram,
                ["variant"] = sourceMaterialAsset.Variant
            };
            PlatformMaterialCookResult cookResult = builder.CookMaterial(new PlatformMaterialCookRequest(
                materialRelativePath.Replace('\\', '/'),
                materialRelativePath.Replace('\\', '/'),
                "psp",
                "debug",
                "psp-forward",
                platformSettings.SchemaId,
                fieldValues));

            MaterialAsset cookedMaterialAsset = Assert.IsType<MaterialAsset>(AssetSerializer.DeserializeFromBytes(cookResult.CookedMaterialBytes));
            MaterialConstantBufferAsset baseColorBuffer = Assert.Single(
                cookedMaterialAsset.ConstantBuffers,
                buffer => buffer.Name == "BaseColorBuffer");
            float4 expectedColor = ParseExpectedColor(ExpectedCubeMaterialColors[cubeIndex]);

            Assert.Equal(expectedColor.X, BitConverter.ToSingle(baseColorBuffer.Data, 0), 5);
            Assert.Equal(expectedColor.Y, BitConverter.ToSingle(baseColorBuffer.Data, 4), 5);
            Assert.Equal(expectedColor.Z, BitConverter.ToSingle(baseColorBuffer.Data, 8), 5);
            Assert.Equal(expectedColor.W, BitConverter.ToSingle(baseColorBuffer.Data, 12), 5);
        }
    }

    /// <summary>
    /// Reads the authored colored cube-grid scene asset from disk.
    /// </summary>
    /// <returns>Deserialized authored scene asset.</returns>
    SceneAsset ReadColoredCubeGridSceneAsset() {
        string scenePath = Path.Combine(CityProjectRootPath, ColoredCubeGridSceneRelativePath);
        Assert.True(File.Exists(scenePath));

        using FileStream stream = File.OpenRead(scenePath);
        return Assert.IsType<SceneAsset>(EditorAssetBinarySerializer.Deserialize(stream));
    }

    /// <summary>
    /// Loads one authored material asset through the material settings service for the PSP platform.
    /// </summary>
    /// <param name="materialPath">Absolute authored material path.</param>
    /// <returns>Resolved PSP runtime-facing material asset.</returns>
    MaterialAsset LoadMaterialAsset(string materialPath) {
        if (string.IsNullOrWhiteSpace(materialPath)) {
            throw new ArgumentException("Material path must be provided.", nameof(materialPath));
        }

        Assert.True(File.Exists(materialPath));
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        return settingsService.LoadMaterialAsset(materialPath, "psp");
    }

    /// <summary>
    /// Parses one expected #RRGGBBAA color string into normalized float channels.
    /// </summary>
    /// <param name="hexColor">Authored color string.</param>
    /// <returns>Normalized color.</returns>
    static float4 ParseExpectedColor(string hexColor) {
        Assert.StartsWith("#", hexColor, StringComparison.Ordinal);
        Assert.Equal(9, hexColor.Length);

        byte red = Convert.ToByte(hexColor.Substring(1, 2), 16);
        byte green = Convert.ToByte(hexColor.Substring(3, 2), 16);
        byte blue = Convert.ToByte(hexColor.Substring(5, 2), 16);
        byte alpha = Convert.ToByte(hexColor.Substring(7, 2), 16);

        return new float4(
            red / 255f,
            green / 255f,
            blue / 255f,
            alpha / 255f);
    }

    /// <summary>
    /// Collects all serialized component records whose type id matches the requested runtime type id.
    /// </summary>
    /// <param name="entities">Scene entities that should be searched recursively.</param>
    /// <param name="componentTypeId">Serialized component type id to match.</param>
    /// <param name="results">Destination list receiving matching component records.</param>
    void CollectComponentsByTypeId(
        SceneEntityAsset[] entities,
        string componentTypeId,
        List<SceneComponentAssetRecord> results) {
        if (entities == null) {
            throw new ArgumentNullException(nameof(entities));
        } else if (string.IsNullOrWhiteSpace(componentTypeId)) {
            throw new ArgumentException("Component type id must be provided.", nameof(componentTypeId));
        } else if (results == null) {
            throw new ArgumentNullException(nameof(results));
        }

        for (int index = 0; index < entities.Length; index++) {
            SceneEntityAsset entity = entities[index];
            if (entity == null) {
                continue;
            }

            for (int componentIndex = 0; componentIndex < entity.Components.Length; componentIndex++) {
                SceneComponentAssetRecord componentRecord = entity.Components[componentIndex];
                if (componentRecord != null && string.Equals(componentRecord.ComponentTypeId, componentTypeId, StringComparison.Ordinal)) {
                    results.Add(componentRecord);
                }
            }

            CollectComponentsByTypeId(entity.Children ?? Array.Empty<SceneEntityAsset>(), componentTypeId, results);
        }
    }

    /// <summary>
    /// Reads one serialized tower-spin component payload using the tagged editor scripted-component payload contract.
    /// </summary>
    /// <param name="componentRecord">Serialized component record to decode.</param>
    /// <returns>Tower-spin component reconstructed from the payload bytes.</returns>
    DirectionalShadowTowerSpinComponent ReadTowerSpinComponent(SceneComponentAssetRecord componentRecord) {
        if (componentRecord == null) {
            throw new ArgumentNullException(nameof(componentRecord));
        }

        EditorTaggedSceneComponentFieldReader reader = new EditorTaggedSceneComponentFieldReader(componentRecord.Payload ?? Array.Empty<byte>());
        DirectionalShadowTowerSpinComponent component = new DirectionalShadowTowerSpinComponent();

        Assert.True(reader.TryGetFieldReader("BaseYawRadians", out EngineBinaryReader baseYawReader));
        using (baseYawReader) {
            component.BaseYawRadians = baseYawReader.ReadSingle();
        }

        Assert.True(reader.TryGetFieldReader("AngularSpeedRadians", out EngineBinaryReader angularSpeedReader));
        using (angularSpeedReader) {
            component.AngularSpeedRadians = angularSpeedReader.ReadSingle();
        }

        return component;
    }
}
