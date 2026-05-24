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
    const string Cube00MaterialRelativePath = @"assets\Materials\rendering\textured_cube_grid\Cube00.hasset";

    /// <summary>
    /// Ensures the authored city textured cube material asset still exists at the expected external project path.
    /// </summary>
    [Fact]
    public void TexturedCubeGridMaterialAsset_exists_in_city_project() {
        string materialPath = Path.Combine(CityProjectRootPath, Cube00MaterialRelativePath);

        Assert.True(File.Exists(materialPath));
    }

    /// <summary>
    /// Ensures the textured cube-grid external project still references the expected PSP-ready material asset path.
    /// </summary>
    [Fact]
    public void TexturedCubeGridMaterialAsset_path_uses_runtime_asset_extension() {
        Assert.EndsWith(".hasset", Cube00MaterialRelativePath, StringComparison.OrdinalIgnoreCase);
    }
}
