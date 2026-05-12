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
    public PspBuildWorkspace(
        string repositoryRootPath,
        string stagingRootPath,
        string generatedCoreRootPath,
        string outputRootPath,
        string nativePbpPath) {
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
    }

    /// <summary>
    /// Gets the PSP repository root used by the native build.
    /// </summary>
    public string RepositoryRootPath { get; }

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
