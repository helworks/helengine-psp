using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using helengine;

namespace helengine.psp.builder;

/// <summary>
/// Implements the PSP platform asset builder contract.
/// </summary>
public sealed class PspPlatformAssetBuilder : IPlatformAssetBuilder {
    /// <summary>
    /// Stable material field identifier used for the authored base color.
    /// </summary>
    const string BaseColorFieldId = "base-color";

    /// <summary>
    /// Stable material field identifier used for the PSP lighting response mode.
    /// </summary>
    const string LightingResponseFieldId = "lighting-response";

    /// <summary>
    /// Stable material field identifier used for the PSP receives-lighting flag.
    /// </summary>
    const string ReceivesLightingFieldId = "receives-lighting";

    /// <summary>
    /// Constant-buffer name used for the authored base color payload.
    /// </summary>
    const string BaseColorBufferName = "BaseColorBuffer";

    /// <summary>
    /// Constant-buffer name used for the PSP lighting configuration payload.
    /// </summary>
    const string LightingConfigBufferName = "LightingConfigBuffer";

    /// <summary>
    /// Environment variable used to override repository-root discovery for PSP builds.
    /// </summary>
    const string RepositoryRootEnvironmentVariableName = "HELENGINE_PSP_REPOSITORY_ROOT";

    /// <summary>
    /// Native PSP build executor used to produce the packaged PBP.
    /// </summary>
    readonly IPspNativeBuildExecutor NativeBuildExecutor;

    /// <summary>
    /// Writes the final PSP homebrew app layout after the native build succeeds.
    /// </summary>
    readonly PspAppLayoutWriter AppLayoutWriter;

    /// <summary>
    /// Normalizes regenerated native-core sources that still need PSP compatibility rewrites before compilation.
    /// </summary>
    readonly PspGeneratedCoreCompatibilityNormalizer GeneratedCoreCompatibilityNormalizer;

    /// <summary>
    /// Initializes the PSP builder with its typed metadata.
    /// </summary>
    public PspPlatformAssetBuilder()
        : this(new PspNativeBuildExecutor()) {
    }

    /// <summary>
    /// Initializes the PSP builder with one explicit native-build executor.
    /// </summary>
    /// <param name="nativeBuildExecutor">Native build executor used by the PSP builder.</param>
    public PspPlatformAssetBuilder(IPspNativeBuildExecutor nativeBuildExecutor) {
        NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
        AppLayoutWriter = new PspAppLayoutWriter();
        GeneratedCoreCompatibilityNormalizer = new PspGeneratedCoreCompatibilityNormalizer();
        Descriptor = new PlatformBuilderDescriptor(
            "helengine.psp.builder",
            "1.0.0",
            "psp",
            new EngineCompatibilityRange("1.0.0", "999.0.0"),
            new ManifestCompatibilityRange(1, 3),
            ["psp"],
            ["debug"]);
        Definition = PspPlatformDefinitionFactory.Create();
    }

    /// <summary>
    /// Gets the explicit builder descriptor for the PSP builder assembly.
    /// </summary>
    public PlatformBuilderDescriptor Descriptor { get; }

    /// <summary>
    /// Gets the typed PSP platform definition exposed to the editor.
    /// </summary>
    public PlatformDefinition Definition { get; }

    /// <summary>
    /// Rejects material cook requests until the PSP runtime material payload is implemented.
    /// </summary>
    /// <param name="request">Material translation request to validate.</param>
    /// <returns>Cooked material result when the runtime payload exists.</returns>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        string shaderAssetId = ReadRequiredField(request.FieldValues, "shader-asset-id");
        string vertexProgram = ReadRequiredField(request.FieldValues, "vertex-program");
        string pixelProgram = ReadRequiredField(request.FieldValues, "pixel-program");
        string variant = ReadRequiredField(request.FieldValues, "variant");
        string baseColor = request.FieldValues.TryGetValue(BaseColorFieldId, out string authoredBaseColor) ? authoredBaseColor : "#ffffff";
        string lightingResponse = request.FieldValues.TryGetValue(LightingResponseFieldId, out string authoredLightingResponse) && !string.IsNullOrWhiteSpace(authoredLightingResponse)
            ? authoredLightingResponse
            : "lit-directional";
        string diffuseTextureAssetId = request.FieldValues.TryGetValue("texture-id", out string authoredTextureAssetId) && !string.IsNullOrWhiteSpace(authoredTextureAssetId)
            ? authoredTextureAssetId
            : string.Empty;
        bool receivesLighting = ReadOptionalBooleanField(request.FieldValues, ReceivesLightingFieldId, true);
        bool castsShadows = ReadOptionalBooleanField(request.FieldValues, "casts-shadow", true);
        bool receivesShadows = ReadOptionalBooleanField(request.FieldValues, "receives-shadow", true);
        float lightingResponseCode = ParseLightingResponseCode(lightingResponse);

        MaterialAsset materialAsset = new MaterialAsset {
            Id = request.MaterialAssetId,
            ShaderAssetId = shaderAssetId,
            VertexProgram = vertexProgram,
            PixelProgram = pixelProgram,
            Variant = variant,
            DiffuseTextureAssetId = diffuseTextureAssetId,
            CastsShadows = castsShadows,
            ReceivesShadows = receivesShadows,
            RenderState = new MaterialRenderState(),
            ConstantBuffers = [
                new MaterialConstantBufferAsset {
                    Name = BaseColorBufferName,
                    Data = CreateFloat4ConstantBufferData(ParseBaseColor(baseColor))
                },
                new MaterialConstantBufferAsset {
                    Name = LightingConfigBufferName,
                    Data = CreateLightingConfigConstantBufferData(receivesLighting, lightingResponseCode)
                }
            ]
        };

        return new PlatformMaterialCookResult(global::helengine.files.AssetSerializer.SerializeToBytes(materialAsset), [shaderAssetId]);
    }

    /// <summary>
    /// Builds the staged PSP homebrew app folder from editor-produced cooked artifacts and generated core output.
    /// </summary>
    /// <param name="request">Resolved build request.</param>
    /// <param name="progressReporter">Progress reporter.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter.</param>
    /// <param name="cancellationToken">Cancellation token.</param>
    /// <returns>Completed build report when the PSP build path exists.</returns>
    public Task<PlatformBuildReport> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        ValidateRequest(request);
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (progressReporter == null) {
            throw new ArgumentNullException(nameof(progressReporter));
        } else if (diagnosticReporter == null) {
            throw new ArgumentNullException(nameof(diagnosticReporter));
        }

        Directory.CreateDirectory(request.OutputRoot);
        Directory.CreateDirectory(request.WorkingRoot);

        List<PlatformBuildDiagnostic> diagnostics = [];
        List<PlatformBuildItemOutcome> sceneOutcomes = BuildSceneOutcomes(request.Manifest.Scenes);
        List<PlatformBuildItemOutcome> looseAssetOutcomes = BuildLooseAssetOutcomes(request.Manifest.LooseAssets);
        string stagingRootPath = Path.Combine(request.WorkingRoot, "psp-staging");

        ResetDirectory(stagingRootPath);
        Directory.CreateDirectory(stagingRootPath);
        StageCookedArtifacts(request, stagingRootPath, diagnostics, diagnosticReporter, progressReporter, cancellationToken);

        if (diagnostics.Count == 0) {
            GeneratedCoreCompatibilityNormalizer.Normalize(request.GeneratedCoreCppRootPath);
            PspBuildWorkspace workspace = CreateWorkspace(request, stagingRootPath);
            NativeBuildExecutor.Build(workspace, cancellationToken);
            AppLayoutWriter.Write(workspace);
            VerifyPackagedOutputs(workspace);
        }

        bool succeeded = diagnostics.Count == 0;
        return Task.FromResult(new PlatformBuildReport(
            succeeded,
            [.. diagnostics],
            [.. sceneOutcomes],
            [.. looseAssetOutcomes]));
    }

    /// <summary>
    /// Stages cooked artifacts into the PSP working tree before the native build runs.
    /// </summary>
    /// <param name="request">Resolved build request.</param>
    /// <param name="stagingRootPath">Working staging root that receives the cooked artifacts.</param>
    /// <param name="diagnostics">Diagnostic sink for staging failures.</param>
    /// <param name="diagnosticReporter">Streaming diagnostic reporter.</param>
    /// <param name="progressReporter">Streaming progress reporter.</param>
    /// <param name="cancellationToken">Cancellation token.</param>
    void StageCookedArtifacts(
        PlatformBuildRequest request,
        string stagingRootPath,
        List<PlatformBuildDiagnostic> diagnostics,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        IPlatformBuildProgressReporter progressReporter,
        CancellationToken cancellationToken) {
        PlatformBuildArtifact[] cookedArtifacts = request.Manifest.CookedArtifacts ?? [];
        for (int artifactIndex = 0; artifactIndex < cookedArtifacts.Length; artifactIndex++) {
            cancellationToken.ThrowIfCancellationRequested();

            PlatformBuildArtifact artifact = cookedArtifacts[artifactIndex];
            string sourcePath = ResolveStagedArtifactSourcePath(artifact.RelativePath);
            if (!File.Exists(sourcePath)) {
                AddDiagnostic(
                    diagnostics,
                    diagnosticReporter,
                    PlatformBuildDiagnosticSeverity.Error,
                    "PSPBUILD001",
                    $"Cooked artifact '{artifact.RelativePath}' was not found in the staged package root.",
                    string.Empty,
                    artifact.LogicalArtifactId,
                    artifact.RelativePath);
                continue;
            }

            string destinationPath = Path.Combine(stagingRootPath, NormalizeRelativePath(artifact.RelativePath));
            string destinationDirectoryPath = Path.GetDirectoryName(destinationPath) ?? throw new InvalidOperationException($"Destination directory could not be resolved for '{destinationPath}'.");
            Directory.CreateDirectory(destinationDirectoryPath);
            File.Copy(sourcePath, destinationPath, true);
            progressReporter.Report(new PlatformBuildProgressUpdate(
                "Stage Cooked Artifacts",
                artifact.LogicalArtifactId,
                artifactIndex + 1,
                cookedArtifacts.Length,
                $"Staged cooked artifact '{artifact.RelativePath}'."));
        }
    }

    /// <summary>
    /// Builds successful scene outcomes for the completed build report.
    /// </summary>
    /// <param name="scenes">Scenes included in the build request.</param>
    /// <returns>Successful scene outcomes for the request.</returns>
    static List<PlatformBuildItemOutcome> BuildSceneOutcomes(PlatformBuildScene[] scenes) {
        List<PlatformBuildItemOutcome> outcomes = [];
        if (scenes == null) {
            return outcomes;
        }

        for (int index = 0; index < scenes.Length; index++) {
            outcomes.Add(new PlatformBuildItemOutcome(scenes[index].SceneId, PlatformBuildItemOutcomeKind.Succeeded));
        }

        return outcomes;
    }

    /// <summary>
    /// Builds successful loose-asset outcomes for the completed build report.
    /// </summary>
    /// <param name="looseAssets">Loose assets included in the build request.</param>
    /// <returns>Successful loose-asset outcomes for the request.</returns>
    static List<PlatformBuildItemOutcome> BuildLooseAssetOutcomes(PlatformBuildAsset[] looseAssets) {
        List<PlatformBuildItemOutcome> outcomes = [];
        if (looseAssets == null) {
            return outcomes;
        }

        for (int index = 0; index < looseAssets.Length; index++) {
            outcomes.Add(new PlatformBuildItemOutcome(looseAssets[index].AssetId, PlatformBuildItemOutcomeKind.Succeeded));
        }

        return outcomes;
    }

    /// <summary>
    /// Adds one structured diagnostic and streams it through the active reporter.
    /// </summary>
    /// <param name="diagnostics">Accumulated diagnostic list.</param>
    /// <param name="diagnosticReporter">Streaming diagnostic reporter.</param>
    /// <param name="severity">Diagnostic severity.</param>
    /// <param name="code">Stable diagnostic code.</param>
    /// <param name="message">Human-readable message.</param>
    /// <param name="sceneId">Associated scene identifier when applicable.</param>
    /// <param name="assetId">Associated asset identifier when applicable.</param>
    /// <param name="sourceIdentity">Associated source identity.</param>
    static void AddDiagnostic(
        List<PlatformBuildDiagnostic> diagnostics,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        PlatformBuildDiagnosticSeverity severity,
        string code,
        string message,
        string sceneId,
        string assetId,
        string sourceIdentity) {
        PlatformBuildDiagnostic diagnostic = new(severity, code, message, sceneId, assetId, sourceIdentity);
        diagnostics.Add(diagnostic);
        diagnosticReporter.Report(diagnostic);
    }

    /// <summary>
    /// Resolves one staged cooked-artifact source path from the builder working directory.
    /// </summary>
    /// <param name="relativePath">Cooked-artifact relative path from the manifest.</param>
    /// <returns>Absolute source path for the staged artifact.</returns>
    static string ResolveStagedArtifactSourcePath(string relativePath) {
        string normalizedRelativePath = NormalizeRelativePath(relativePath);
        return Path.GetFullPath(Path.Combine(Directory.GetCurrentDirectory(), normalizedRelativePath));
    }

    /// <summary>
    /// Removes one directory when it already exists.
    /// </summary>
    /// <param name="path">Directory path to recreate.</param>
    static void ResetDirectory(string path) {
        if (Directory.Exists(path)) {
            Directory.Delete(path, true);
        }
    }

    /// <summary>
    /// Verifies the final PSP homebrew layout contains the produced PBP.
    /// </summary>
    /// <param name="workspace">Workspace describing the PSP output layout.</param>
    static void VerifyPackagedOutputs(PspBuildWorkspace workspace) {
        if (workspace == null) {
            throw new ArgumentNullException(nameof(workspace));
        }

        if (!File.Exists(workspace.AppEbootPath)) {
            throw new FileNotFoundException("PSP EBOOT.PBP was not produced.", workspace.AppEbootPath);
        }
    }

    /// <summary>
    /// Normalizes one logical runtime path into the current filesystem separator convention.
    /// </summary>
    /// <param name="path">Logical path to normalize.</param>
    /// <returns>Normalized filesystem path.</returns>
    static string NormalizeRelativePath(string path) {
        return path.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
    }

    /// <summary>
    /// Resolves the PSP repository root from the environment or the builder assembly location.
    /// </summary>
    /// <returns>Absolute PSP repository root path.</returns>
    static string ResolveRepositoryRootPath() {
        string configuredRepositoryRootPath = Environment.GetEnvironmentVariable(RepositoryRootEnvironmentVariableName) ?? string.Empty;
        if (IsRepositoryRootPath(configuredRepositoryRootPath)) {
            return Path.GetFullPath(configuredRepositoryRootPath);
        }

        string currentPath = AppContext.BaseDirectory;
        while (!string.IsNullOrWhiteSpace(currentPath)) {
            if (IsRepositoryRootPath(currentPath)) {
                return currentPath;
            }

            DirectoryInfo parentDirectory = Directory.GetParent(currentPath);
            if (parentDirectory == null) {
                break;
            }

            currentPath = parentDirectory.FullName;
        }

        throw new InvalidOperationException("Could not resolve the helengine-psp repository root from the builder assembly location.");
    }

    /// <summary>
    /// Determines whether one path points at the PSP repository root.
    /// </summary>
    /// <param name="path">Candidate repository root path.</param>
    /// <returns>True when the path looks like the PSP repository root.</returns>
    static bool IsRepositoryRootPath(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            return false;
        }

        string makefilePath = Path.Combine(path, "Makefile");
        string bootHostPath = Path.Combine(path, "src", "platform", "psp", "PspBootHost.cpp");
        return File.Exists(makefilePath) && File.Exists(bootHostPath);
    }

    /// <summary>
    /// Creates the prepared PSP workspace for the native-build and app-layout phases.
    /// </summary>
    /// <param name="request">Resolved build request.</param>
    /// <param name="stagingRootPath">Working staging root that already contains the cooked artifacts.</param>
    /// <returns>Prepared PSP build workspace.</returns>
    static PspBuildWorkspace CreateWorkspace(PlatformBuildRequest request, string stagingRootPath) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (string.IsNullOrWhiteSpace(stagingRootPath)) {
            throw new ArgumentException("Staging root path must be provided.", nameof(stagingRootPath));
        }

        string repositoryRootPath = ResolveRepositoryRootPath();
        string nativePbpPath = Path.Combine(repositoryRootPath, "build", "EBOOT.PBP");
        return new PspBuildWorkspace(
            repositoryRootPath,
            stagingRootPath,
            request.GeneratedCoreCppRootPath,
            request.OutputRoot,
            nativePbpPath);
    }

    /// <summary>
    /// Validates the PSP build request before any filesystem work begins.
    /// </summary>
    /// <param name="request">Resolved build request.</param>
    static void ValidateRequest(PlatformBuildRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (string.IsNullOrWhiteSpace(request.GeneratedCoreCppRootPath)) {
            throw new ArgumentException("Generated core root path must be provided for PSP builds.", nameof(request));
        } else if (!Directory.Exists(request.GeneratedCoreCppRootPath)) {
            throw new DirectoryNotFoundException($"Generated core root '{request.GeneratedCoreCppRootPath}' was not found.");
        }

        ValidateGeneratedCoreContract(request.GeneratedCoreCppRootPath);
        ValidateStartupScene(request.Manifest);
    }

    /// <summary>
    /// Verifies the generated-core root contains the runtime manifest sources required by the PSP native build.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated-core root path supplied by the editor build graph.</param>
    static void ValidateGeneratedCoreContract(string generatedCoreRootPath) {
        if (!File.Exists(Path.Combine(generatedCoreRootPath, "helcpp_config.hpp"))) {
            throw new FileNotFoundException("Generated core root does not contain helcpp_config.hpp.", Path.Combine(generatedCoreRootPath, "helcpp_config.hpp"));
        } else if (!File.Exists(Path.Combine(generatedCoreRootPath, "helengine_core_amalgamated.cpp"))) {
            throw new FileNotFoundException("Generated core root does not contain helengine_core_amalgamated.cpp.", Path.Combine(generatedCoreRootPath, "helengine_core_amalgamated.cpp"));
        } else if (!File.Exists(Path.Combine(generatedCoreRootPath, "runtime", "runtime_startup_manifest.cpp"))) {
            throw new FileNotFoundException("Generated core root does not contain runtime/runtime_startup_manifest.cpp.", Path.Combine(generatedCoreRootPath, "runtime", "runtime_startup_manifest.cpp"));
        } else if (!File.Exists(Path.Combine(generatedCoreRootPath, "runtime", "runtime_scene_catalog_manifest.cpp"))) {
            throw new FileNotFoundException("Generated core root does not contain runtime/runtime_scene_catalog_manifest.cpp.", Path.Combine(generatedCoreRootPath, "runtime", "runtime_scene_catalog_manifest.cpp"));
        }
    }

    /// <summary>
    /// Verifies the build manifest contains the configured startup scene as one of the built cooked scenes.
    /// </summary>
    /// <param name="manifest">Build manifest supplied by the editor build graph.</param>
    static void ValidateStartupScene(PlatformBuildManifest manifest) {
        if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        } else if (string.IsNullOrWhiteSpace(manifest.StartupSceneId)) {
            throw new InvalidOperationException("PSP builds require a startup scene id.");
        } else if (manifest.Scenes == null || manifest.Scenes.Length == 0) {
            throw new InvalidOperationException("PSP builds require at least one cooked scene entry containing the startup scene.");
        }

        for (int index = 0; index < manifest.Scenes.Length; index++) {
            PlatformBuildScene scene = manifest.Scenes[index];
            if (string.Equals(scene.SceneId, manifest.StartupSceneId, StringComparison.OrdinalIgnoreCase)) {
                return;
            }
        }

        throw new InvalidOperationException($"PSP startup scene '{manifest.StartupSceneId}' was not present in the cooked scene manifest.");
    }

    /// <summary>
    /// Reads one required material field from the builder-owned field map.
    /// </summary>
    /// <param name="fieldValues">Serialized material field values keyed by field id.</param>
    /// <param name="fieldId">Field identifier to read.</param>
    /// <returns>Resolved field value.</returns>
    static string ReadRequiredField(IReadOnlyDictionary<string, string> fieldValues, string fieldId) {
        if (fieldValues == null) {
            throw new ArgumentNullException(nameof(fieldValues));
        } else if (string.IsNullOrWhiteSpace(fieldId)) {
            throw new ArgumentException("Field id must be provided.", nameof(fieldId));
        }

        string value;
        if (!fieldValues.TryGetValue(fieldId, out value) || string.IsNullOrWhiteSpace(value)) {
            throw new InvalidOperationException($"Missing required material field '{fieldId}'.");
        }

        return value;
    }

    /// <summary>
    /// Reads an optional boolean material field from the builder-owned field map.
    /// </summary>
    /// <param name="fieldValues">Serialized material field values keyed by field id.</param>
    /// <param name="fieldId">Field identifier to read.</param>
    /// <param name="defaultValue">Value returned when the field is missing or blank.</param>
    /// <returns>Resolved boolean value.</returns>
    static bool ReadOptionalBooleanField(IReadOnlyDictionary<string, string> fieldValues, string fieldId, bool defaultValue) {
        if (fieldValues == null) {
            throw new ArgumentNullException(nameof(fieldValues));
        } else if (string.IsNullOrWhiteSpace(fieldId)) {
            throw new ArgumentException("Field id must be provided.", nameof(fieldId));
        }

        string value;
        if (!fieldValues.TryGetValue(fieldId, out value) || string.IsNullOrWhiteSpace(value)) {
            return defaultValue;
        }

        if (!bool.TryParse(value, out bool parsedValue)) {
            throw new InvalidOperationException($"Material field '{fieldId}' must be a boolean value.");
        }

        return parsedValue;
    }

    /// <summary>
    /// Parses one serialized base-color string into a normalized floating-point color.
    /// </summary>
    /// <param name="serializedColor">Serialized color string in <c>#RRGGBB</c> or <c>#RRGGBBAA</c> form.</param>
    /// <returns>Normalized color value.</returns>
    static float4 ParseBaseColor(string serializedColor) {
        if (string.IsNullOrWhiteSpace(serializedColor)) {
            return new float4(1f, 1f, 1f, 1f);
        }

        string normalized = serializedColor.Trim();
        if (normalized.StartsWith("#", StringComparison.Ordinal)) {
            normalized = normalized.Substring(1);
        }

        if (normalized.Length != 6 && normalized.Length != 8) {
            throw new InvalidOperationException("Base color must use #RRGGBB or #RRGGBBAA.");
        }

        try {
            byte alpha = 255;
            int offset = 0;
            if (normalized.Length == 8) {
                alpha = Convert.ToByte(normalized.Substring(0, 2), 16);
                offset = 2;
            }

            byte red = Convert.ToByte(normalized.Substring(offset, 2), 16);
            byte green = Convert.ToByte(normalized.Substring(offset + 2, 2), 16);
            byte blue = Convert.ToByte(normalized.Substring(offset + 4, 2), 16);

            return new float4(
                red / 255f,
                green / 255f,
                blue / 255f,
                alpha / 255f);
        } catch (FormatException ex) {
            throw new InvalidOperationException("Base color must use #RRGGBB or #RRGGBBAA.", ex);
        }
    }

    /// <summary>
    /// Packs one floating-point color into a 16-byte constant-buffer payload.
    /// </summary>
    /// <param name="value">Normalized color value to encode.</param>
    /// <returns>Packed constant-buffer bytes.</returns>
    static byte[] CreateFloat4ConstantBufferData(float4 value) {
        using MemoryStream stream = new MemoryStream();
        using EngineBinaryWriter writer = EngineBinaryWriter.Create(stream, EngineBinaryEndianness.LittleEndian);
        writer.WriteSingle(value.X);
        writer.WriteSingle(value.Y);
        writer.WriteSingle(value.Z);
        writer.WriteSingle(value.W);
        return stream.ToArray();
    }

    /// <summary>
    /// Resolves the cooked numeric code used by the PSP runtime material lighting-response contract.
    /// </summary>
    /// <param name="serializedLightingResponse">Serialized lighting response value.</param>
    /// <returns>Cooked numeric response code.</returns>
    static float ParseLightingResponseCode(string serializedLightingResponse) {
        string normalized = string.IsNullOrWhiteSpace(serializedLightingResponse)
            ? "lit-directional"
            : serializedLightingResponse.Trim().ToLowerInvariant();

        if (normalized == "unlit") {
            return 0.0f;
        } else if (normalized == "lit-directional") {
            return 1.0f;
        }

        throw new InvalidOperationException("Lighting response must be 'unlit' or 'lit-directional'.");
    }

    /// <summary>
    /// Packs the PSP lighting configuration into one 16-byte constant-buffer payload.
    /// </summary>
    /// <param name="receivesLighting">Whether the material receives scene lighting.</param>
    /// <param name="lightingResponseCode">Cooked numeric lighting-response code.</param>
    /// <returns>Packed lighting configuration bytes.</returns>
    static byte[] CreateLightingConfigConstantBufferData(bool receivesLighting, float lightingResponseCode) {
        using MemoryStream stream = new MemoryStream();
        using EngineBinaryWriter writer = EngineBinaryWriter.Create(stream, EngineBinaryEndianness.LittleEndian);
        writer.WriteSingle(receivesLighting ? 1.0f : 0.0f);
        writer.WriteSingle(lightingResponseCode);
        writer.WriteSingle(0.0f);
        writer.WriteSingle(0.0f);
        return stream.ToArray();
    }
}
