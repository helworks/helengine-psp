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
    /// Ensures the PSP runtime texture cache pads authored textures to a GU-friendly buffer width instead of uploading NPOT row strides directly.
    /// </summary>
    [Fact]
    public void PspTextureCache_pads_runtime_textures_to_gu_friendly_buffer_width() {
        string runtimeTextureHeaderPath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRuntimeTexture.hpp");
        string textureCacheSourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspTextureCache.cpp");
        string runtimeTextureHeader = File.ReadAllText(runtimeTextureHeaderPath);
        string textureCacheSource = File.ReadAllText(textureCacheSourcePath);

        Assert.Contains("std::uint16_t GetTextureBufferWidth() const;", runtimeTextureHeader, StringComparison.Ordinal);
        Assert.Contains("void SetTextureBufferWidth(std::uint16_t textureBufferWidth);", runtimeTextureHeader, StringComparison.Ordinal);
        Assert.Contains("CalculatePaddedTextureBufferWidth", textureCacheSource, StringComparison.Ordinal);
        Assert.Contains("runtimeTexture->SetTextureBufferWidth", textureCacheSource, StringComparison.Ordinal);
        Assert.Contains("CopyPixelsToPaddedBufferWidth", textureCacheSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP 2D and 3D renderers bind GU textures using the padded buffer width instead of the authored NPOT asset width as the row stride.
    /// </summary>
    [Fact]
    public void PspRenderManagers_bind_textures_using_padded_buffer_width() {
        string renderManager2DPath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager2D.cpp");
        string renderManager3DPath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager3D.cpp");
        string renderManager2DSource = File.ReadAllText(renderManager2DPath);
        string renderManager3DSource = File.ReadAllText(renderManager3DPath);

        Assert.Contains("texture->GetTextureBufferWidth()", renderManager2DSource, StringComparison.Ordinal);
        Assert.DoesNotContain("sceGuTexImage(0, texture->get_Width(), texture->get_Height(), texture->get_Width()", renderManager2DSource, StringComparison.Ordinal);
        Assert.Contains("texture->GetTextureBufferWidth()", renderManager3DSource, StringComparison.Ordinal);
        Assert.DoesNotContain("sceGuTexImage(0, texture->get_Width(), texture->get_Height(), texture->get_Width()", renderManager3DSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP 2D sprite renderer applies entity scale and rotation instead of drawing every sprite axis-aligned.
    /// </summary>
    [Fact]
    public void PspRenderManager2D_applies_sprite_scale_and_rotation_from_entity_transform() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager2D.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("float3 scale = parent->get_Scale();", source, StringComparison.Ordinal);
        Assert.Contains("float3 rotatedRight = float4::RotateVector(float3::get_UnitX(), parent->get_Orientation());", source, StringComparison.Ordinal);
        Assert.Contains("DrawTexturedQuadTransformed(", source, StringComparison.Ordinal);
        Assert.Contains("DrawTexturedTriangles(vertices, 6, texture);", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP text renderer resolves right and center alignment from measured visible line widths instead of always drawing from the left edge.
    /// </summary>
    [Fact]
    public void PspRenderManager2D_applies_text_alignment_using_visible_line_widths() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager2D.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("TextLayoutAlignmentUtils::MeasureVisibleLineWidth", source, StringComparison.Ordinal);
        Assert.Contains("TextLayoutAlignmentUtils::ResolveHorizontalOffset", source, StringComparison.Ordinal);
        Assert.Contains("text->get_Alignment()", source, StringComparison.Ordinal);
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
    /// Ensures the PSP boot host applies the generated standard-platform input manifest to core initialization.
    /// </summary>
    [Fact]
    public void PspBootHost_applies_standard_platform_input_manifest_to_core_initialization() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("#include \"runtime/runtime_standard_platform_input_manifest.hpp\"", source, StringComparison.Ordinal);
        Assert.Contains("::StandardPlatformInputConfiguration* BuildStandardPlatformInputConfiguration()", source, StringComparison.Ordinal);
        Assert.Contains("const HERuntimeStandardPlatformActionEntry* manifestEntries = he_runtime_standard_platform_action_entries(&count);", source, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_StandardPlatformInputConfiguration(BuildStandardPlatformInputConfiguration());", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP boot host registers the 3D physics runtime after core initialization so packaged physics scenes simulate after loading.
    /// </summary>
    [Fact]
    public void PspBootHost_registers_physics3d_runtime_after_core_initialization() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("#include \"BepuPhysicsWorld3D.hpp\"", source, StringComparison.Ordinal);
        Assert.Contains("#include \"BepuRuntimeComponentRegistration.hpp\"", source, StringComparison.Ordinal);
        Assert.Contains("BepuRuntimeComponentRegistration::AttachRuntimeWorld(EngineCore, physicsWorld);", source, StringComparison.Ordinal);
        Assert.Contains("BepuRuntimeComponentRegistration::RegisterSceneBinding(EngineCore);", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP boot host applies the agreed low-precision PSP physics preset instead of inheriting desktop-oriented defaults.
    /// </summary>
    [Fact]
    public void PspBootHost_uses_low_precision_psp_physics_runtime_tuning() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("EngineOptions->set_PhysicsFixedStepSeconds(1.0 / 30.0);", source, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_PhysicsMaxStepsPerUpdate(2);", source, StringComparison.Ordinal);
        Assert.Contains("#include \"BepuPhysicsWorld3D.hpp\"", source, StringComparison.Ordinal);
        Assert.Contains("#include \"BepuRuntimeComponentRegistration.hpp\"", source, StringComparison.Ordinal);
        Assert.Contains("BepuPhysicsWorld3D::CreateWithSolveSchedule(2, 1)", source, StringComparison.Ordinal);
        Assert.Contains("BepuRuntimeComponentRegistration::AttachRuntimeWorld(EngineCore, physicsWorld);", source, StringComparison.Ordinal);
        Assert.Contains("BepuRuntimeComponentRegistration::RegisterSceneBinding(EngineCore);", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP boot host does not enable per-frame return-transition file tracing that would stall menu returns.
    /// </summary>
    [Fact]
    public void PspBootHost_does_not_enable_per_frame_return_transition_tracing() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.DoesNotContain("ReturnTransitionTraceFramesRemaining", source, StringComparison.Ordinal);
        Assert.DoesNotContain("WasReturnButtonDownLastFrame", source, StringComparison.Ordinal);
        Assert.DoesNotContain("TraceRuntimeTransitionState(", source, StringComparison.Ordinal);
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
