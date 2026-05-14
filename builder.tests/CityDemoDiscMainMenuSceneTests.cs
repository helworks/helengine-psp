namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the city demo disc main menu export includes the scene and font payload required by the PSP runtime.
/// </summary>
public sealed class CityDemoDiscMainMenuSceneTests {
    /// <summary>
    /// Absolute PSP export root used by the demo menu packaging regression.
    /// </summary>
    const string DemoDiscMenuPackageRootPath = @"C:\tmp\city-psp-demo-disc-menu-build\PSP\GAME\HELENGINE";

    /// <summary>
    /// Ensures the packaged PSP demo menu export contains the cooked menu scene and both menu fonts.
    /// </summary>
    [Fact]
    public void DemoDiscMainMenu_export_includes_scene_and_fonts() {
        string scenePath = Path.Combine(DemoDiscMenuPackageRootPath, @"cooked\scenes\DemoDiscMainMenu.hasset");
        string titleFontPath = Path.Combine(DemoDiscMenuPackageRootPath, @"cooked\Fonts\DemoDiscTitle.hefont");
        string bodyFontPath = Path.Combine(DemoDiscMenuPackageRootPath, @"cooked\Fonts\DemoDiscBody.hefont");

        Assert.True(File.Exists(scenePath), $"Expected cooked menu scene at {scenePath}.");
        Assert.True(File.Exists(titleFontPath), $"Expected cooked title font at {titleFontPath}.");
        Assert.True(File.Exists(bodyFontPath), $"Expected cooked body font at {bodyFontPath}.");
    }
}
