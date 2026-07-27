namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the packaged PSP runtime sources accept the editor-generated unity translation unit contract.
/// </summary>
public sealed class PspPackagedRuntimeSourceTests {
    /// <summary>
    /// Ensures the PSP homebrew runtime reads packaged content through its explicit memory-card stream source instead of the pruned host file system runtime feature.
    /// </summary>
    [Fact]
    public void PspBootHost_uses_memory_card_content_stream_source() {
        string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
        string bootHostPath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "PspBootHost.cpp");
        string contentStreamSourceHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "PspMemoryCardContentStreamSource.hpp");
        string contentStreamSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "PspMemoryCardContentStreamSource.cpp");
        string cmakePath = Path.Combine(repositoryRootPath, "CMakeLists.txt");

        string bootHostSource = File.ReadAllText(bootHostPath);
        string contentStreamSourceHeader = File.ReadAllText(contentStreamSourceHeaderPath);
        string contentStreamSource = File.ReadAllText(contentStreamSourcePath);
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.Contains("#include \"platform/psp/PspMemoryCardContentStreamSource.hpp\"", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("new PspMemoryCardContentStreamSource(appRootPath)", bootHostSource, StringComparison.Ordinal);
        Assert.DoesNotContain("HostFileSystemContentStreamSource", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("class PspMemoryCardContentStreamSource final : public ::IContentStreamSource", contentStreamSourceHeader, StringComparison.Ordinal);
        Assert.Contains("::Stream* OpenRead(std::string assetPath) override;", contentStreamSourceHeader, StringComparison.Ordinal);
        Assert.Contains("new FileStream(ResolvePhysicalPath(assetPath), FileMode::Open, FileAccess::Read, FileShare::Read)", contentStreamSource, StringComparison.Ordinal);
        Assert.Contains("src/platform/psp/PspMemoryCardContentStreamSource.cpp", cmakeSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP fixed-function 3D renderer consumes platform-owned cooked materials without retaining shader runtime interfaces.
    /// </summary>
    [Fact]
    public void PspRenderManager3D_uses_cooked_fixed_function_materials_without_shader_interfaces() {
        string sourcePath = Path.Combine(PspRepositoryPathResolver.ResolveRepositoryRootPath(), "src", "platform", "psp", "rendering", "PspRenderManager3D.hpp");
        string source = File.ReadAllText(sourcePath);

        Assert.DoesNotContain("IShaderRenderManager3D", source, StringComparison.Ordinal);
        Assert.DoesNotContain("ShaderAsset.hpp", source, StringComparison.Ordinal);
        Assert.Contains("BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset) override", source, StringComparison.Ordinal);
        Assert.Contains("BuildMaterialFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP 3D renderer materializes platform-owned cooked model payloads through the runtime content stream source.
    /// </summary>
    [Fact]
    public void PspRenderManager3D_builds_cooked_models_from_content_streams() {
        string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
        string headerPath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "rendering", "PspRenderManager3D.hpp");
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "rendering", "PspRenderManager3D.cpp");

        string header = File.ReadAllText(headerPath);
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("BuildModelFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override", header, StringComparison.Ordinal);
        Assert.Contains("Stream* stream = contentStreamSource->OpenRead(cookedAssetPath);", source, StringComparison.Ordinal);
        Assert.Contains("AssetSerializer::Deserialize(stream)", source, StringComparison.Ordinal);
        Assert.Contains("dynamic_cast<ModelAsset*>(asset)", source, StringComparison.Ordinal);
        Assert.Contains("BuildModelFromRaw(modelAsset)", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP 2D renderer materializes cooked texture payloads through the runtime content stream source.
    /// </summary>
    [Fact]
    public void PspRenderManager2D_builds_cooked_textures_from_content_streams() {
        string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
        string headerPath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "rendering", "PspRenderManager2D.hpp");
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "rendering", "PspRenderManager2D.cpp");

        string header = File.ReadAllText(headerPath);
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("BuildTextureFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override", header, StringComparison.Ordinal);
        Assert.Contains("Stream* stream = contentStreamSource->OpenRead(cookedAssetPath);", source, StringComparison.Ordinal);
        Assert.Contains("AssetSerializer::Deserialize(stream)", source, StringComparison.Ordinal);
        Assert.Contains("dynamic_cast<TextureAsset*>(asset)", source, StringComparison.Ordinal);
        Assert.Contains("TextureCache.BuildTextureFromRaw(textureAsset)", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP fixed-function material path loads, binds, and later releases a texture referenced by its cooked material payload.
    /// </summary>
    [Fact]
    public void PspRenderManager3D_loads_and_binds_the_cooked_material_texture() {
        string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
        string renderManagerSourcePath = Path.Combine(
            repositoryRootPath,
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager3D.cpp");
        string runtimeMaterialHeaderPath = Path.Combine(
            repositoryRootPath,
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRuntimeMaterial.hpp");
        string renderManagerSource = File.ReadAllText(renderManagerSourcePath);
        string runtimeMaterialHeader = File.ReadAllText(runtimeMaterialHeaderPath);

        Assert.Contains("std::string(\"cooked/imported/\") + materialAsset->TextureRelativePath", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("RenderManager2D->BuildTextureFromCooked(texturePath, contentStreamSource)", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("pspMaterial->SetPrimaryTexture(texture)", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("RuntimeTexture* GetOwnedTexture() const;", runtimeMaterialHeader, StringComparison.Ordinal);
        Assert.Contains("RenderManager2D->ReleaseTexture(pspMaterial->GetOwnedTexture())", renderManagerSource, StringComparison.Ordinal);
    }

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
    /// Ensures the PSP fixed-function text path submits the shared shadow, cardinal-outline, and fill sequence with per-pass colors.
    /// </summary>
    [Fact]
    public void PspRenderManager2D_draws_text_shadow_and_outline_effect_passes() {
        string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
        string headerPath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "rendering", "PspRenderManager2D.hpp");
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "rendering", "PspRenderManager2D.cpp");
        string header = File.ReadAllText(headerPath);
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("DrawTextEffectPasses(text, cacheEntry)", source, StringComparison.Ordinal);
        Assert.Contains("text->get_ShadowOffset()", source, StringComparison.Ordinal);
        Assert.Contains("text->get_OutlineScale()", source, StringComparison.Ordinal);
        Assert.Contains("text->get_ShadowColor()", source, StringComparison.Ordinal);
        Assert.Contains("text->get_OutlineColor()", source, StringComparison.Ordinal);
        Assert.Contains("text->get_Color()", source, StringComparison.Ordinal);
        Assert.Contains("float2(-outlineScale, 0.0f)", source, StringComparison.Ordinal);
        Assert.Contains("float2(outlineScale, 0.0f)", source, StringComparison.Ordinal);
        Assert.Contains("float2(0.0f, -outlineScale)", source, StringComparison.Ordinal);
        Assert.Contains("float2(0.0f, outlineScale)", source, StringComparison.Ordinal);
        Assert.Contains("const byte4* colorOverride", header, StringComparison.Ordinal);
        Assert.Contains("drawVertices[index].Color = ConvertColorToAbgr(*colorOverride)", source, StringComparison.Ordinal);
        Assert.Contains("atlasPixels[(sampleY * atlasWidth) + sampleX]", source, StringComparison.Ordinal);
        Assert.DoesNotContain("cacheEntry.Color);\n                        BlendAbgrPixel", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the fixed-function PSP text path keeps glyphs and all effect passes on the same integral pixel grid.
    /// </summary>
    [Fact]
    public void PspRenderManager2D_snaps_text_effect_passes_to_the_pixel_grid() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager2D.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("SnapTextCoordinateToPixel(text->get_Parent()->get_Position())", source, StringComparison.Ordinal);
        Assert.Contains("SnapOutlineOffsetToPixelGrid(outlineScale)", source, StringComparison.Ordinal);
        Assert.Contains("float2(-outlineOffset, 0.0f)", source, StringComparison.Ordinal);
        Assert.Contains("float2(outlineOffset, 0.0f)", source, StringComparison.Ordinal);
        Assert.Contains("std::max(1.0f, std::round(std::abs(offset)))", source, StringComparison.Ordinal);
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
    /// Ensures the PSP runtime masks hardware floating-point exceptions so managed IEEE arithmetic can produce infinities and NaNs without terminating the process.
    /// </summary>
    [Fact]
    public void PspBootHost_masks_hardware_floating_point_exceptions() {
        string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
        string bootHostPath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "PspBootHost.cpp");
        string cmakePath = Path.Combine(repositoryRootPath, "CMakeLists.txt");

        string bootHostSource = File.ReadAllText(bootHostPath);
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.Contains("#include <pspfpu.h>", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("pspFpuSetEnable(0);", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("ConfigureFloatingPointEnvironment();", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("MaximumPhysicsStageRecordCount = 0", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("pspfpu", cmakeSource, StringComparison.Ordinal);
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

        Assert.Contains("EngineOptions->set_PhysicsFixedStepSeconds(1.0 / 12.0);", source, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_PhysicsMaxStepsPerUpdate(1);", source, StringComparison.Ordinal);
        Assert.Contains("#include \"BepuPhysicsWorld3D.hpp\"", source, StringComparison.Ordinal);
        Assert.Contains("#include \"BepuRuntimeComponentRegistration.hpp\"", source, StringComparison.Ordinal);
        Assert.Contains("BepuPhysicsWorld3D::CreateWithSolveSchedule(1, 1)", source, StringComparison.Ordinal);
        Assert.Contains("BepuRuntimeComponentRegistration::AttachRuntimeWorld(EngineCore, physicsWorld);", source, StringComparison.Ordinal);
        Assert.Contains("BepuRuntimeComponentRegistration::RegisterSceneBinding(EngineCore);", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP runtime uses one BEPU velocity iteration and skips static or sleeping body synchronization work.
    /// </summary>
    [Fact]
    public void PspPhysics_uses_one_velocity_iteration_and_skips_unnecessary_sync() {
        string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
        string bootSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "PspBootHost.cpp");
        string worldSourcePath = Path.Combine(repositoryRootPath, "..", "helengine", "engine", "helengine.bepu", "BepuPhysicsWorld3D.cs");
        string bootSource = File.ReadAllText(bootSourcePath);
        string worldSource = File.ReadAllText(worldSourcePath);

        Assert.Contains("BepuPhysicsWorld3D::CreateWithSolveSchedule(1, 1)", bootSource, StringComparison.Ordinal);
        Assert.Contains("if (!handle.HasBodyHandle || handle.IsStatic)", worldSource, StringComparison.Ordinal);
        Assert.Contains("if (handle.IsDynamic && !bodyReference.Awake)", worldSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP boot log receives each generated BEPU scene-binding diagnostic so hardware crashes can be localized to one body registration boundary.
    /// </summary>
    [Fact]
    public void PspBootHost_forwards_bepu_scene_binding_diagnostics_to_boot_trace() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("physicsWorld->set_SceneBindingDiagnosticSink(", source, StringComparison.Ordinal);
        Assert.Contains("new Action<std::string>([](std::string message) {", source, StringComparison.Ordinal);
        Assert.Contains("PspBootTrace::WriteLine(std::string(\"PhysicsBinding \") + message);", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP boot host arms a bounded all-stage trace exactly when scene physics binding completes.
    /// </summary>
    [Fact]
    public void PspBootHost_records_post_scene_binding_update_stage_diagnostics() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("class PspPhysicsUpdateStageDiagnosticsProvider final", source, StringComparison.Ordinal);
        Assert.Contains("public ::IRuntimeUpdateStageDiagnosticsProvider", source, StringComparison.Ordinal);
        Assert.Contains("PspBootTrace::WriteLine(std::string(\"PhysicsUpdateStage \") + stage);", source, StringComparison.Ordinal);
        Assert.Contains("MaximumPhysicsStageRecordCount = 512", source, StringComparison.Ordinal);
        Assert.Contains("void BeginPostSceneBindingTrace()", source, StringComparison.Ordinal);
        Assert.Contains("PostSceneBindingTraceDiagnosticsProvider->BeginPostSceneBindingTrace();", source, StringComparison.Ordinal);
        Assert.Contains("rendering::PspRenderManager3D::BeginPostPhysicsBindingDrawTrace();", source, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_RuntimeDiagnosticsProvider(PostSceneBindingTraceDiagnosticsProvider);", source, StringComparison.Ordinal);
        Assert.Contains("new Action<std::string>([](std::string message) {", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures scaled fixed-function vertices are retained in normal heap memory instead of consuming the GU display-list allocator.
    /// </summary>
    [Fact]
    public void PspRenderManager3D_uses_heap_buffers_for_scaled_fixed_function_vertices() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager3D.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("new PspRuntimeModel::FixedFunctionVertex[static_cast<std::size_t>(vertexCount)]", source, StringComparison.Ordinal);
        Assert.Contains("FrameScaledFixedFunctionVertexBuffers.push_back(vertices);", source, StringComparison.Ordinal);
        Assert.Contains("ReleaseFrameScaledFixedFunctionVertexBuffers();", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures textured fixed-function Course meshes retain their transient vertex streams in heap memory instead of exhausting the GU display-list allocator.
    /// </summary>
    [Fact]
    public void PspRenderManager3D_uses_heap_buffers_for_transient_textured_fixed_function_vertices() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager3D.cpp");
        string headerPath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager3D.hpp");
        string source = File.ReadAllText(sourcePath);
        string header = File.ReadAllText(headerPath);

        Assert.Contains("new PspRuntimeModel::FixedFunctionTexturedVertex[static_cast<std::size_t>(vertexCount)]", source, StringComparison.Ordinal);
        Assert.Contains("FrameTransientFixedFunctionTexturedVertexBuffers.push_back(vertices);", source, StringComparison.Ordinal);
        Assert.Contains("ReleaseFrameTransientFixedFunctionTexturedVertexBuffers();", source, StringComparison.Ordinal);
        Assert.Contains("std::vector<PspRuntimeModel::FixedFunctionTexturedVertex*> FrameTransientFixedFunctionTexturedVertexBuffers;", header, StringComparison.Ordinal);
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
    /// Ensures the PSP app heap budget leaves headroom for scene swaps that overlap two large cooked music tracks.
    /// </summary>
    [Fact]
    public void PspBootHost_uses_expanded_heap_budget_for_scene_music_transitions() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("PSP_HEAP_SIZE_KB(24 * 1024);", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP boot host wires one native audio backend into generated core before scene audio sources start playback.
    /// </summary>
    [Fact]
    public void PspBootHost_wires_generated_core_audio_backend() {
        string headerPath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.hpp");
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string header = File.ReadAllText(headerPath);
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("class IAudioBackend;", header, StringComparison.Ordinal);
        Assert.Contains("::IAudioBackend* EngineAudioBackend;", header, StringComparison.Ordinal);
        Assert.Contains("#include \"IAudioBackend.hpp\"", source, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/psp/audio/PspAudioBackend.hpp\"", source, StringComparison.Ordinal);
        Assert.Contains("EngineAudioBackend = new PspAudioBackend();", source, StringComparison.Ordinal);
        Assert.Contains("EngineCore->SetAudioBackend(EngineAudioBackend);", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP boot host registers the kernel exit callback and breaks both frame loops when the Home button requests shutdown.
    /// </summary>
    [Fact]
    public void PspBootHost_registers_home_button_exit_callback_and_exits_cleanly() {
        string headerPath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.hpp");
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "PspBootHost.cpp");
        string header = File.ReadAllText(headerPath);
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("std::atomic<bool> ExitRequested;", header, StringComparison.Ordinal);
        Assert.Contains("void RegisterExitCallback();", header, StringComparison.Ordinal);
        Assert.Contains("static int HandleExitCallback", header, StringComparison.Ordinal);
        Assert.Contains("static int ExitCallbackThreadEntry", header, StringComparison.Ordinal);
        Assert.Contains("RegisterExitCallback();", source, StringComparison.Ordinal);
        Assert.Contains("sceKernelCreateCallback(", source, StringComparison.Ordinal);
        Assert.Contains("sceKernelRegisterExitCallback", source, StringComparison.Ordinal);
        Assert.Contains("sceKernelSleepThreadCB();", source, StringComparison.Ordinal);
        Assert.Contains("while (!IsExitRequested())", source, StringComparison.Ordinal);
        Assert.Contains("sceKernelExitGame();", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the PSP native build compiles and links the dedicated audio backend used by scene-authored menu and showcase music.
    /// </summary>
    [Fact]
    public void CMakeLists_compiles_and_links_the_psp_audio_backend() {
        string cmakePath = Path.Combine(PspRepositoryPathResolver.ResolveRepositoryRootPath(), "CMakeLists.txt");
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.Contains("src/platform/psp/audio/PspAudioBackend.cpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("pspaudio", cmakeSource, StringComparison.Ordinal);
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
        Assert.DoesNotContain("texture->Dispose();", source, StringComparison.Ordinal);
        Assert.DoesNotContain("delete texture;", source, StringComparison.Ordinal);
        Assert.DoesNotContain("delete sourceTextureAsset->Colors;", source, StringComparison.Ordinal);
        Assert.DoesNotContain("delete font->get_FontInfo();", source, StringComparison.Ordinal);
        Assert.DoesNotContain("delete font->get_Characters();", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures PSP 2D texture release owns the full runtime-texture lifetime instead of leaking scene-owned sprite textures between scene swaps.
    /// </summary>
    [Fact]
    public void PspRenderManager2D_release_texture_disposes_and_deletes_runtime_textures() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager2D.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("TextureCache.ReleaseTexture(pspTexture);", source, StringComparison.Ordinal);
        Assert.Contains("pspTexture->Dispose();", source, StringComparison.Ordinal);
        Assert.Contains("delete pspTexture;", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures PSP 3D runtime model and material releases actually dispose and delete native allocations during repeated scene swaps.
    /// </summary>
    [Fact]
    public void PspRenderManager3D_release_paths_dispose_and_delete_runtime_assets() {
        string sourcePath = Path.Combine(
            PspRepositoryPathResolver.ResolveRepositoryRootPath(),
            "src",
            "platform",
            "psp",
            "rendering",
            "PspRenderManager3D.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("model->Dispose();", source, StringComparison.Ordinal);
        Assert.Contains("delete model;", source, StringComparison.Ordinal);
        Assert.Contains("material->Dispose();", source, StringComparison.Ordinal);
        Assert.Contains("delete material;", source, StringComparison.Ordinal);
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
