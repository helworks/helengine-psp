namespace helengine.psp.builder;

/// <summary>
/// Describes the prepared filesystem layout consumed by the native PSP build and final app packaging.
/// </summary>
public sealed class PspBuildWorkspace {
    /// <summary>
    /// PSP homebrew directory name used by the staged app.
    /// </summary>
    public const string GameDirectoryName = "HELENGINE";

    /// <summary>
    /// Initializes one PSP build workspace.
    /// </summary>
    /// <param name="repositoryRootPath">PSP repository root used by the native build.</param>
    /// <param name="stagingRootPath">Temporary staging root that contains cooked artifacts.</param>
    /// <param name="generatedCoreRootPath">Generated-core C++ root provided by the editor build graph.</param>
    /// <param name="outputRootPath">Requested builder output root.</param>
    /// <param name="nativePbpPath">Path where the native build emits the intermediate PBP.</param>
    /// <param name="nativeObjectCacheRootPath">Persistent compiler and CMake cache root for this project and profile.</param>
    public PspBuildWorkspace(
        string repositoryRootPath,
        string stagingRootPath,
        string generatedCoreRootPath,
        string outputRootPath,
        string nativePbpPath,
        string nativeObjectCacheRootPath = null) {
        if (string.IsNullOrWhiteSpace(repositoryRootPath)) {
            throw new ArgumentException("Repository root path is required.", nameof(repositoryRootPath));
        } else if (string.IsNullOrWhiteSpace(stagingRootPath)) {
            throw new ArgumentException("Staging root path is required.", nameof(stagingRootPath));
        } else if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path is required.", nameof(generatedCoreRootPath));
        } else if (string.IsNullOrWhiteSpace(outputRootPath)) {
            throw new ArgumentException("Output root path is required.", nameof(outputRootPath));
        } else if (string.IsNullOrWhiteSpace(nativePbpPath)) {
            throw new ArgumentException("Native PBP path is required.", nameof(nativePbpPath));
        }

        RepositoryRootPath = Path.GetFullPath(repositoryRootPath);
        StagingRootPath = Path.GetFullPath(stagingRootPath);
        GeneratedCoreRootPath = Path.GetFullPath(generatedCoreRootPath);
        OutputRootPath = Path.GetFullPath(outputRootPath);
        NativePbpPath = Path.GetFullPath(nativePbpPath);
        NativeObjectCacheRootPath = string.IsNullOrWhiteSpace(nativeObjectCacheRootPath)
            ? WorkingCacheRootFallback(repositoryRootPath)
            : Path.GetFullPath(nativeObjectCacheRootPath);
    }

    /// <summary>
    /// Gets the PSP repository root used by the native build.
    /// </summary>
    public string RepositoryRootPath { get; }

    /// <summary>
    /// Gets or sets the game title stamped into the packaged EBOOT PARAM.SFO; empty keeps the toolchain default.
    /// </summary>
    public string GameName { get; set; } = string.Empty;

    /// <summary>
    /// Gets the temporary staging root that contains cooked artifacts.
    /// </summary>
    public string StagingRootPath { get; }

    /// <summary>
    /// Gets the generated-core C++ root provided by the editor build graph.
    /// </summary>
    public string GeneratedCoreRootPath { get; }

    /// <summary>
    /// Gets the requested builder output root.
    /// </summary>
    public string OutputRootPath { get; }

    /// <summary>
    /// Gets the path where the native build emits the intermediate PBP.
    /// </summary>
    public string NativePbpPath { get; }

    /// <summary>
    /// Gets the stable directory mounted as the PSP native CMake and object cache.
    /// </summary>
    public string NativeObjectCacheRootPath { get; }

    /// <summary>
    /// Gets the fresh package directory that receives the PBP from the native build.
    /// </summary>
    public string NativePackageRootPath => Path.GetDirectoryName(NativePbpPath) ?? throw new InvalidOperationException("Native PBP package directory could not be resolved.");

    /// <summary>
    /// Uses a repository-local fallback for callers that do not provide the editor cache root.
    /// </summary>
    /// <param name="repositoryRootPath">PSP repository root.</param>
    /// <returns>Absolute fallback cache root.</returns>
    static string WorkingCacheRootFallback(string repositoryRootPath) {
        return Path.Combine(repositoryRootPath, "cache", "build", "psp", "default");
    }

    /// <summary>
    /// Gets the final PSP homebrew app root.
    /// </summary>
    public string AppRootPath => Path.Combine(OutputRootPath, "PSP", "GAME", GameDirectoryName);

    /// <summary>
    /// Gets the cooked-assets root inside the final PSP app layout.
    /// </summary>
    public string CookedOutputRootPath => Path.Combine(AppRootPath, "cooked");

    /// <summary>
    /// Gets the final packaged EBOOT path inside the PSP app layout.
    /// </summary>
    public string AppEbootPath => Path.Combine(AppRootPath, "EBOOT.PBP");
}
