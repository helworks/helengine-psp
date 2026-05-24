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
    /// Stable serialized type id for the authored axis-rotation gameplay component.
    /// </summary>
    const string AxisRotationComponentTypeId = "gameplay.rendering.AxisRotationComponent, gameplay";

    /// <summary>
    /// Relative authored material path for the first colored cube material.
    /// </summary>
    const string Cube00MaterialRelativePath = @"assets\Materials\rendering\colored_cube_grid\Cube00.hasset";

    /// <summary>
    /// Ensures the authored colored cube-grid scene still contains one automatic gameplay rotation payload for each cube entity.
    /// </summary>
    [Fact]
    public void ColoredCubeGridScene_includes_gameplay_owned_axis_rotation_components() {
        SceneAsset sceneAsset = ReadColoredCubeGridSceneAsset();
        List<SceneComponentAssetRecord> axisRotationRecords = new List<SceneComponentAssetRecord>();

        CollectComponentsByTypeId(
            sceneAsset.RootEntities ?? Array.Empty<SceneEntityAsset>(),
            AxisRotationComponentTypeId,
            axisRotationRecords);

        Assert.Equal(16, axisRotationRecords.Count);
        for (int index = 0; index < axisRotationRecords.Count; index++) {
            Assert.NotNull(axisRotationRecords[index]);
            Assert.False(string.IsNullOrWhiteSpace(axisRotationRecords[index].ComponentTypeId));
            Assert.NotEmpty(axisRotationRecords[index].Payload ?? Array.Empty<byte>());
        }
    }

    /// <summary>
    /// Ensures the authored colored cube-grid scene writes the demo-disc return-to-menu component using a non-empty automatic scripted runtime payload.
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
        Assert.Equal(ReturnToMenuComponentTypeId, returnToMenuRecord.ComponentTypeId);
        Assert.NotEmpty(returnToMenuRecord.Payload ?? Array.Empty<byte>());
    }

    /// <summary>
    /// Ensures the authored city colored cube material asset still exists at the expected external project path.
    /// </summary>
    [Fact]
    public void ColoredCubeGridMaterialAsset_exists_in_city_project() {
        string materialPath = Path.Combine(CityProjectRootPath, Cube00MaterialRelativePath);

        Assert.True(File.Exists(materialPath));
    }

    /// <summary>
    /// Ensures the colored cube-grid external project still references the expected PSP-ready material asset path.
    /// </summary>
    [Fact]
    public void ColoredCubeGridMaterialAsset_path_uses_runtime_asset_extension() {
        Assert.EndsWith(".hasset", Cube00MaterialRelativePath, StringComparison.OrdinalIgnoreCase);
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
}
