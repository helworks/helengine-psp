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
        string sourcePath = Path.Combine(PspRepositoryPathResolver.ResolveRepositoryRootPath(), "CMakeLists.txt");
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
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspTextureCache.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.DoesNotContain("width * height * 4", source, StringComparison.Ordinal);
        Assert.Contains("TextureAssetColorFormat::Rgba4444", source, StringComparison.Ordinal);
        Assert.Contains("TextureAssetColorFormat::Indexed8", source, StringComparison.Ordinal);
        Assert.Contains("ConvertTextureToAbgr8888", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP texture cache only references texture color formats that still exist in the generated runtime enum.
    /// </summary>
    [Fact]
    public void PspTextureCache_does_not_reference_removed_texture_color_formats() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspTextureCache.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.DoesNotContain("TextureAssetColorFormat::GxRgb5A3", source, StringComparison.Ordinal);
        Assert.Contains("TextureAssetColorFormat::Rgba4444", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP boot host supports a local startup-scene override file for packaged runtime diagnostics.
    /// </summary>
    [Fact]
    public void PspBootHost_supports_startup_scene_override_file() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("startup_scene_override.txt", source, StringComparison.Ordinal);
        Assert.Contains("StartupSceneOverride sceneId=", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP boot host traces draw-phase boundaries during focused return-transition diagnostics.
    /// </summary>
    [Fact]
    public void PspBootHost_traces_before_and_after_draw_during_transition_window() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("\"BeforeDraw\"", source, StringComparison.Ordinal);
        Assert.Contains("\"AfterDraw\"", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP display list is large enough for menu-heavy 2D return frames.
    /// </summary>
    [Fact]
    public void PspBootHost_uses_large_display_list_for_menu_return_frames() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("DisplayListByteCount = 0x80000", source, StringComparison.Ordinal);
        Assert.Contains("DisplayListStorage[DisplayListByteCount / sizeof(unsigned int)]", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures PSP font release follows the engine disposal contract instead of manually deleting partial native state.
    /// </summary>
    [Fact]
    public void PspRenderManager2D_release_font_uses_font_dispose_contract() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager2D.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("font->Dispose();", source, StringComparison.Ordinal);
        Assert.Contains("ReleaseTexture(texture);", source, StringComparison.Ordinal);
        Assert.Contains("texture->Dispose();", source, StringComparison.Ordinal);
        Assert.Contains("delete texture;", source, StringComparison.Ordinal);
        Assert.DoesNotContain("delete sourceTextureAsset->Colors;", source, StringComparison.Ordinal);
        Assert.DoesNotContain("delete font->get_FontInfo();", source, StringComparison.Ordinal);
        Assert.DoesNotContain("delete font->get_Characters();", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures consecutive PSP 2D drawables under the same clip rectangle can stay batched.
    /// </summary>
    [Fact]
    public void PspRenderManager2D_does_not_flush_white_batch_when_clip_rect_is_unchanged() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager2D.cpp");
        string source = File.ReadAllText(sourcePath);

        int unchangedClipReturnIndex = source.IndexOf("ActiveClipRect.W == clipRect.W) {\n            return;", StringComparison.Ordinal);
        int flushIndex = source.IndexOf("FlushPendingWhiteTriangles();\n\n        const int left", StringComparison.Ordinal);

        Assert.True(unchangedClipReturnIndex >= 0, "ApplyClipRect should return when the active clip rectangle is unchanged.");
        Assert.True(flushIndex > unchangedClipReturnIndex, "ApplyClipRect should flush pending white geometry only after it knows the clip rectangle changed.");
    }

    /// <summary>
    /// Ensures deferred PSP texture release holds pixel buffers instead of stale runtime-texture pointers.
    /// </summary>
    [Fact]
    public void PspTextureCache_defers_pixel_buffers_not_runtime_texture_instances() {
        string headerPath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspTextureCache.hpp");
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspTextureCache.cpp");
        string header = File.ReadAllText(headerPath);
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("TakePixelsAbgr8888", source, StringComparison.Ordinal);
        Assert.Contains("std::vector<std::vector<std::uint32_t>> ReleasedTexturePixelBuffers", header, StringComparison.Ordinal);
        Assert.DoesNotContain("std::vector<PspRuntimeTexture*> ReleasedTextures", header, StringComparison.Ordinal);
        Assert.DoesNotContain("delete texture;", source, StringComparison.Ordinal);
    }
}
