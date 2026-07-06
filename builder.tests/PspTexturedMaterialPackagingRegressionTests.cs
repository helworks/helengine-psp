using helengine.baseplatform.Requests;
using helengine.editor;
using helengine.psp.builder;
using System.Reflection;

namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies PSP scene packaging preserves diffuse texture bindings across multi-material textured scenes.
/// </summary>
public sealed class PspTexturedMaterialPackagingRegressionTests {
    /// <summary>
    /// Stable scene identifier used by the multi-material textured-scene regression.
    /// </summary>
    const string SceneId = "Scenes/TexturedScene.helen";

    /// <summary>
    /// Ensures packaging sixteen textured PSP materials preserves every authored diffuse texture binding in the cooked material payloads.
    /// </summary>
    [Fact]
    public void Package_WhenSceneReferencesSixteenTexturedPspMaterials_PreservesDiffuseTextureBindingForEveryCookedMaterial() {
        string workspaceRootPath = Path.Combine(Path.GetTempPath(), "helengine-psp-textured-material-packaging", Guid.NewGuid().ToString("N"));
        string projectRootPath = workspaceRootPath;
        string buildRootPath = Path.Combine(workspaceRootPath, "Build");
        Directory.CreateDirectory(Path.Combine(projectRootPath, "assets"));
        Directory.CreateDirectory(Path.Combine(projectRootPath, "cache", "shader-cache"));
        Directory.CreateDirectory(buildRootPath);

        try {
            Dictionary<string, string> expectedTextureAssetIdsByMaterialPath = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            for (int cubeIndex = 0; cubeIndex < 16; cubeIndex++) {
                string textureRelativePath = CreateTextureRelativePath(cubeIndex);
                string materialRelativePath = CreateMaterialRelativePath(cubeIndex);
                string textureAssetId = WriteSourceTextureAssetAndReturnAssetId(projectRootPath, textureRelativePath, "psp", [(byte)cubeIndex, (byte)(cubeIndex + 1), (byte)(cubeIndex + 2), (byte)(cubeIndex + 3)]);
                WritePspMaterialAsset(projectRootPath, materialRelativePath, textureAssetId);
                expectedTextureAssetIdsByMaterialPath[materialRelativePath] = textureAssetId;
            }

            WriteSceneAssetWithSceneLevelMaterialReferences(projectRootPath, SceneId, expectedTextureAssetIdsByMaterialPath.Keys.ToArray());
            SeedBuiltInStandardShaderAsset(ShaderCompileTarget.DirectX11);

            PspPlatformAssetBuilder builder = new(
                new FakePspNativeBuildExecutor(),
                new FakePspPlatformCookSourceProcessor(
                    new TextureAsset {
                        Width = 1,
                        Height = 1,
                        Colors = [255, 255, 255, 255]
                    },
                    CreateTestFontAsset()));

            EditorPlatformBuildScenePackager packager = new(
                projectRootPath,
                [
                    new TextureImporterRegistration("test-texture", new PspBuilderTestTextureImporter(), [".png"])
                ],
                builder.Definition,
                CreateTestFontAsset(),
                builder,
                "debug",
                "psp-forward");

            EditorPlatformBuildScenePackagerResult result = packager.Package([SceneId], buildRootPath);

            Assert.Equal(16, result.PlatformCookWorkItems.Count);

            foreach ((string materialRelativePath, string expectedTextureAssetId) in expectedTextureAssetIdsByMaterialPath) {
                string cookedMaterialPath = Path.Combine(buildRootPath, "cooked", materialRelativePath.Replace('/', Path.DirectorySeparatorChar));
                using FileStream stream = new FileStream(cookedMaterialPath, FileMode.Open, FileAccess.Read, FileShare.Read);
                ShaderMaterialAsset cookedMaterialAsset = Assert.IsType<ShaderMaterialAsset>(global::helengine.editor.AssetSerializer.Deserialize(stream));
                Assert.Equal(expectedTextureAssetId, cookedMaterialAsset.DiffuseTextureAssetId);
            }
        } finally {
            try {
                if (Directory.Exists(workspaceRootPath)) {
                    Directory.Delete(workspaceRootPath, true);
                }
            } catch {
            }
        }
    }

    /// <summary>
    /// Builds the stable project-relative texture source path for one cube index.
    /// </summary>
    /// <param name="cubeIndex">Zero-based cube index.</param>
    /// <returns>Project-relative texture source path.</returns>
    static string CreateTextureRelativePath(int cubeIndex) {
        return $"Textures/Cube{cubeIndex:00}.png";
    }

    /// <summary>
    /// Builds the stable project-relative material path for one cube index.
    /// </summary>
    /// <param name="cubeIndex">Zero-based cube index.</param>
    /// <returns>Project-relative material path.</returns>
    static string CreateMaterialRelativePath(int cubeIndex) {
        return $"Materials/rendering/textured_cube_grid/Cube{cubeIndex:00}.hasset";
    }

    /// <summary>
    /// Writes one serialized scene asset that references each supplied material through the scene-level asset-reference list.
    /// </summary>
    /// <param name="projectRootPath">Temporary editor project root path.</param>
    /// <param name="sceneId">Stable scene identifier to write.</param>
    /// <param name="materialRelativePaths">Project-relative material paths referenced by the scene.</param>
    static void WriteSceneAssetWithSceneLevelMaterialReferences(
        string projectRootPath,
        string sceneId,
        IReadOnlyList<string> materialRelativePaths) {
        string scenePath = Path.Combine(projectRootPath, "assets", sceneId.Replace('/', Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(scenePath)!);

        SceneAssetReference[] materialReferences = new SceneAssetReference[materialRelativePaths.Count];
        for (int index = 0; index < materialRelativePaths.Count; index++) {
            materialReferences[index] = global::helengine.SceneAssetReferenceFactory.CreateFileSystemMaterial(materialRelativePaths[index]);
        }

        SceneAsset sceneAsset = new SceneAsset {
            Id = sceneId,
            AssetReferences = materialReferences,
            RootEntities = Array.Empty<SceneEntityAsset>()
        };

        using FileStream stream = new FileStream(scenePath, FileMode.Create, FileAccess.Write, FileShare.None);
        global::helengine.editor.AssetSerializer.Serialize(stream, sceneAsset);
    }

    /// <summary>
    /// Seeds the built-in shader cache with one precompiled standard shader so packaging does not require runtime shader compilation.
    /// </summary>
    /// <param name="target">Shader compile target whose cache entry should be seeded.</param>
    static void SeedBuiltInStandardShaderAsset(ShaderCompileTarget target) {
        string shaderPath = EditorBuiltInShaderAssetLibrary.ResolveShaderPath("ForwardStandardShader.hlsl");
        string cacheKey = string.Concat(target.ToString(), "|", shaderPath);
        ShaderAsset shaderAsset = new ShaderAsset {
            Id = "ForwardStandardShader",
            Name = "ForwardStandardShader",
            TargetName = ShaderTargetNames.GetTargetName(target),
            Programs = [
                new ShaderProgramAsset {
                    Name = "ForwardStandardShader.vs",
                    Stage = ShaderStage.Vertex,
                    EntryPoint = "VS",
                    Bindings = Array.Empty<ShaderBindingAsset>(),
                    Inputs = Array.Empty<ShaderVertexElementAsset>(),
                    Outputs = Array.Empty<ShaderVertexElementAsset>(),
                    Variants = [
                        new ShaderVariantAsset {
                            Name = "default",
                            Defines = Array.Empty<string>()
                        }
                    ]
                }
            ],
            Binaries = [
                new ShaderBinaryAsset {
                    ProgramName = "ForwardStandardShader.vs",
                    Stage = ShaderStage.Vertex,
                    TargetName = ShaderTargetNames.GetTargetName(target),
                    Variant = "default",
                    Bytecode = [1, 2, 3, 4]
                }
            ]
        };

        FieldInfo cacheField = typeof(EditorBuiltInShaderAssetLibrary).GetField("ShaderAssetsByKey", BindingFlags.Static | BindingFlags.NonPublic) ?? throw new InvalidOperationException("Built-in shader cache field was not found.");
        Dictionary<string, ShaderAsset> cache = Assert.IsType<Dictionary<string, ShaderAsset>>(cacheField.GetValue(null));
        cache[cacheKey] = shaderAsset;
    }

    /// <summary>
    /// Creates a minimal font asset used by the scene packager when it resolves fallback UI content.
    /// </summary>
    /// <returns>Deterministic test font asset.</returns>
    static FontAsset CreateTestFontAsset() {
        FontAsset fontAsset = new FontAsset(
            new FontInfo("TestFont", 16, 4f),
            new FakeRuntimeTexture {
                Width = 1,
                Height = 1
            },
            new Dictionary<char, FontChar> {
                ['A'] = new FontChar {
                    SourceRect = new float4(0f, 0f, 1f, 1f),
                    OffsetY = 0f,
                    AdvanceWidth = 8f,
                    BearingX = 0f,
                    BearingY = 0f
                }
            },
            16f,
            1,
            1);
        fontAsset.SourceTextureAsset = new TextureAsset {
            Id = "fonts/default.hefont#atlas",
            RuntimeAssetId = RuntimeAssetIdGenerator.Generate("fonts/default.hefont#atlas"),
            Width = 1,
            Height = 1,
            ColorFormat = TextureAssetColorFormat.Rgba4444,
            AlphaPrecision = TextureAssetAlphaPrecision.A4,
            Colors = [0xFF, 0x0F]
        };
        return fontAsset;
    }

    /// <summary>
    /// Writes one source texture file and resolves its imported asset id through the editor import pipeline.
    /// </summary>
    /// <param name="projectRootPath">Temporary editor project root path.</param>
    /// <param name="textureRelativePath">Project-relative texture source path to create.</param>
    /// <param name="platformId">Platform identifier used to resolve texture import settings.</param>
    /// <param name="sourceBytes">Deterministic source bytes written to the texture file.</param>
    /// <returns>Resolved imported texture asset id.</returns>
    static string WriteSourceTextureAssetAndReturnAssetId(string projectRootPath, string textureRelativePath, string platformId, byte[] sourceBytes) {
        string textureSourcePath = Path.Combine(projectRootPath, "assets", textureRelativePath.Replace('/', Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(textureSourcePath)!);
        File.WriteAllBytes(textureSourcePath, sourceBytes);

        ContentManager contentManager = new ContentManager(projectRootPath);
        AssetImportManager assetImportManager = new AssetImportManager(projectRootPath, contentManager);
        assetImportManager.CurrentPlatformId = platformId;
        assetImportManager.RegisterTextureImporter(new TextureImporterRegistration("test-texture", new PspBuilderTestTextureImporter(), [".png"]));

        TextureAssetImportSettings settings;
        Assert.True(assetImportManager.TryLoadOrCreateTextureImportSettings(textureSourcePath, out settings));
        Assert.NotNull(settings);
        Assert.NotNull(settings.Importer);
        Assert.False(string.IsNullOrWhiteSpace(settings.Importer.AssetId));
        return settings.Importer.AssetId;
    }

    /// <summary>
    /// Writes one authored PSP material settings document that references the supplied imported diffuse texture id.
    /// </summary>
    /// <param name="projectRootPath">Temporary editor project root path.</param>
    /// <param name="materialRelativePath">Project-relative material path to write.</param>
    /// <param name="diffuseTextureAssetId">Imported diffuse texture asset id referenced by the material.</param>
    static void WritePspMaterialAsset(string projectRootPath, string materialRelativePath, string diffuseTextureAssetId) {
        string materialPath = Path.Combine(projectRootPath, "assets", materialRelativePath.Replace('/', Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(materialPath)!);

        MaterialAssetImportSettings settings = new MaterialAssetImportSettings {
            Importer = new AssetImporterSettings {
                ImporterId = "helengine.material",
                SourceChecksum = string.Empty,
                AssetId = materialRelativePath
            },
            Processor = new MaterialAssetProcessorPlatformSettings()
        };
        settings.Processor.Platforms["psp"] = new MaterialAssetProcessorSettings {
            SchemaId = "standard-shader",
            FieldValues = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) {
                ["use-custom-shader"] = "false",
                ["shader-asset-id"] = "engine:material:standard",
                ["vertex-program"] = "ForwardStandard.vs",
                ["pixel-program"] = "ForwardStandard.ps",
                ["variant"] = "Mesh",
                ["texture-id"] = diffuseTextureAssetId,
                ["casts-shadow"] = "true",
                ["receives-shadow"] = "true",
                ["base-color"] = "#FFFFFFFF"
            }
        };

        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        settingsService.Save(materialPath, settings);
    }
}
