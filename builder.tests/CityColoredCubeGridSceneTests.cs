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
    /// Relative authored material path for the first colored cube material.
    /// </summary>
    const string Cube00MaterialRelativePath = @"assets\Materials\rendering\colored_cube_grid\Cube00.helmat";

    /// <summary>
    /// Ensures the authored colored cube-grid scene encodes the engine-owned tower-spin payload using the runtime binary contract.
    /// </summary>
    [Fact]
    public void ColoredCubeGridScene_writes_engine_owned_tower_spin_payload() {
        SceneAsset sceneAsset = ReadColoredCubeGridSceneAsset();
        List<SceneComponentAssetRecord> towerSpinRecords = new List<SceneComponentAssetRecord>();

        CollectComponentsByTypeId(
            sceneAsset.RootEntities ?? Array.Empty<SceneEntityAsset>(),
            DirectionalShadowTowerSpinComponent.SerializedComponentTypeId,
            towerSpinRecords);

        Assert.Equal(16, towerSpinRecords.Count);
        for (int index = 0; index < towerSpinRecords.Count; index++) {
            DirectionalShadowTowerSpinComponent component = ReadTowerSpinComponent(towerSpinRecords[index]);

            Assert.Equal(0f, component.BaseYawRadians, 5);
            Assert.Equal((float)(Math.PI / 2.0), component.AngularSpeedRadians, 5);
        }
    }

    /// <summary>
    /// Ensures the authored colored cube-grid scene writes the demo-disc return-to-menu component using the automatic scripted runtime payload contract.
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
        using MemoryStream stream = new MemoryStream(returnToMenuRecord.Payload ?? Array.Empty<byte>(), false);
        using EngineBinaryReader reader = EngineBinaryReader.Create(stream, EngineBinaryEndianness.LittleEndian);
        byte version = reader.ReadByte();
        int memberCount = reader.ReadInt32();
        byte updateOrder = reader.ReadByte();

        Assert.Equal(AutomaticScriptComponentRuntimeDeserializer.CurrentVersion, version);
        Assert.Equal(1, memberCount);
        Assert.Equal(0, updateOrder);
    }

    /// <summary>
    /// Ensures the generated city colored cube material sidecar includes PSP material settings with the authored base color.
    /// </summary>
    [Fact]
    public void ColoredCubeGridMaterialSettings_include_psp_base_color() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        string materialPath = Path.Combine(CityProjectRootPath, Cube00MaterialRelativePath);

        Assert.True(settingsService.TryLoad(materialPath, out MaterialAssetImportSettings settings));
        Assert.NotNull(settings);
        Assert.True(settings.Processor.Platforms.ContainsKey("psp"));
        Assert.Equal("standard-shader", settings.Processor.Platforms["psp"].SchemaId);
        Assert.Equal("#FF4040FF", settings.Processor.Platforms["psp"].FieldValues["base-color"]);
    }

    /// <summary>
    /// Ensures the generated city colored cube material cooks into a PSP base-color buffer that preserves the authored color.
    /// </summary>
    [Fact]
    public void ColoredCubeGridMaterialCook_preserves_psp_base_color() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        string materialPath = Path.Combine(CityProjectRootPath, Cube00MaterialRelativePath);

        Assert.True(settingsService.TryLoad(materialPath, out MaterialAssetImportSettings settings));
        Assert.NotNull(settings);
        Assert.True(settings.Processor.Platforms.ContainsKey("psp"));

        MaterialAsset sourceMaterialAsset = ReadColoredCubeGridMaterialAsset();
        MaterialAssetProcessorSettings platformSettings = settings.Processor.Platforms["psp"];
        Dictionary<string, string> fieldValues = new Dictionary<string, string>(platformSettings.FieldValues) {
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
            platformSettings.SchemaId,
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
    /// Reads the authored first colored-cube material asset from disk.
    /// </summary>
    /// <returns>Deserialized authored material asset.</returns>
    MaterialAsset ReadColoredCubeGridMaterialAsset() {
        string materialPath = Path.Combine(CityProjectRootPath, Cube00MaterialRelativePath);
        Assert.True(File.Exists(materialPath));

        using FileStream stream = File.OpenRead(materialPath);
        return Assert.IsType<MaterialAsset>(EditorAssetBinarySerializer.Deserialize(stream));
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
    /// Reads one serialized tower-spin component payload using the engine runtime binary contract.
    /// </summary>
    /// <param name="componentRecord">Serialized component record to decode.</param>
    /// <returns>Tower-spin component reconstructed from the payload bytes.</returns>
    DirectionalShadowTowerSpinComponent ReadTowerSpinComponent(SceneComponentAssetRecord componentRecord) {
        if (componentRecord == null) {
            throw new ArgumentNullException(nameof(componentRecord));
        }

        using MemoryStream stream = new MemoryStream(componentRecord.Payload ?? Array.Empty<byte>(), false);
        using EngineBinaryReader reader = EngineBinaryReader.Create(stream, EngineBinaryEndianness.LittleEndian);
        byte version = reader.ReadByte();
        Assert.Equal(DirectionalShadowMotionComponentScenePayloadSerializer.CurrentVersion, version);
        return DirectionalShadowMotionComponentScenePayloadSerializer.ReadTowerSpin(reader);
    }
}
