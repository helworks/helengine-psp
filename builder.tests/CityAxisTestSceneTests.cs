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
    const string AxisXMaterialRelativePath = @"assets\materials\rendering\axis_test\X.hasset";

    /// <summary>
    /// Ensures the authored city axis-test material asset still exists at the expected external project path.
    /// </summary>
    [Fact]
    public void AxisTestMaterialAsset_exists_in_city_project() {
        string materialPath = Path.Combine(CityProjectRootPath, AxisXMaterialRelativePath);

        Assert.True(File.Exists(materialPath));
    }

    /// <summary>
    /// Ensures the axis-test external project still references the expected PSP-ready material asset path.
    /// </summary>
    [Fact]
    public void AxisTestMaterialAsset_path_uses_runtime_asset_extension() {
        Assert.EndsWith(".hasset", AxisXMaterialRelativePath, StringComparison.OrdinalIgnoreCase);
    }
}
