namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the packaged PSP runtime sources accept the editor-generated unity translation unit contract.
/// </summary>
public sealed class PspPackagedRuntimeSourceTests {
    /// <summary>
    /// Ensures the PSP CMake entrypoint accepts both the legacy amalgamated file and the current unity file names.
    /// </summary>
    [Fact]
    public void CMakeLists_accepts_both_generated_core_translation_unit_names() {
        string sourcePath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "CMakeLists.txt"));
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("cxx_std_20", source, StringComparison.Ordinal);
        Assert.Contains("helengine_core_amalgamated.cpp", source, StringComparison.Ordinal);
        Assert.Contains("helengine_core_unity.cpp", source, StringComparison.Ordinal);
        Assert.Contains("does not contain helengine_core_unity.cpp or helengine_core_amalgamated.cpp", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP texture cache decodes cooked platform texture formats instead of assuming raw RGBA32 payloads.
    /// </summary>
    [Fact]
    public void PspTextureCache_decodes_cooked_texture_formats() {
        string sourcePath = Path.Combine(
            AppContext.BaseDirectory,
            "..",
            "..",
            "..",
            "..",
            "src",
            "platform",
            "psp",
            "rendering",
            "PspTextureCache.cpp");
        string source = File.ReadAllText(Path.GetFullPath(sourcePath));

        Assert.DoesNotContain("width * height * 4", source, StringComparison.Ordinal);
        Assert.Contains("TextureAssetColorFormat::Rgba4444", source, StringComparison.Ordinal);
        Assert.Contains("TextureAssetColorFormat::Indexed8", source, StringComparison.Ordinal);
        Assert.Contains("ConvertTextureToAbgr8888", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures PSP font release follows the engine disposal contract instead of manually deleting partial native state.
    /// </summary>
    [Fact]
    public void PspRenderManager2D_release_font_uses_font_dispose_contract() {
        string sourcePath = Path.Combine(
            AppContext.BaseDirectory,
            "..",
            "..",
            "..",
            "..",
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager2D.cpp");
        string source = File.ReadAllText(Path.GetFullPath(sourcePath));

        Assert.Contains("font->Dispose();", source, StringComparison.Ordinal);
        Assert.Contains("ReleaseTexture(texture);", source, StringComparison.Ordinal);
        Assert.Contains("texture->Dispose();", source, StringComparison.Ordinal);
        Assert.Contains("delete texture;", source, StringComparison.Ordinal);
        Assert.DoesNotContain("delete sourceTextureAsset->Colors;", source, StringComparison.Ordinal);
        Assert.DoesNotContain("delete font->get_FontInfo();", source, StringComparison.Ordinal);
        Assert.DoesNotContain("delete font->get_Characters();", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures deferred PSP texture release holds pixel buffers instead of stale runtime-texture pointers.
    /// </summary>
    [Fact]
    public void PspTextureCache_defers_pixel_buffers_not_runtime_texture_instances() {
        string headerPath = Path.Combine(
            AppContext.BaseDirectory,
            "..",
            "..",
            "..",
            "..",
            "src",
            "platform",
            "psp",
            "rendering",
            "PspTextureCache.hpp");
        string sourcePath = Path.Combine(
            AppContext.BaseDirectory,
            "..",
            "..",
            "..",
            "..",
            "src",
            "platform",
            "psp",
            "rendering",
            "PspTextureCache.cpp");
        string header = File.ReadAllText(Path.GetFullPath(headerPath));
        string source = File.ReadAllText(Path.GetFullPath(sourcePath));

        Assert.Contains("TakePixelsAbgr8888", source, StringComparison.Ordinal);
        Assert.Contains("std::vector<std::vector<std::uint32_t>> ReleasedTexturePixelBuffers", header, StringComparison.Ordinal);
        Assert.DoesNotContain("std::vector<PspRuntimeTexture*> ReleasedTextures", header, StringComparison.Ordinal);
        Assert.DoesNotContain("delete texture;", source, StringComparison.Ordinal);
    }
}
