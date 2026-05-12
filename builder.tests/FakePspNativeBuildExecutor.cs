namespace helengine.psp.builder.tests;

/// <summary>
/// Simulates the native PSP build by writing one fake PBP file for staging tests.
/// </summary>
public sealed class FakePspNativeBuildExecutor : IPspNativeBuildExecutor {
    /// <summary>
    /// Gets the last workspace received by the fake executor.
    /// </summary>
    public PspBuildWorkspace LastWorkspace { get; private set; }

    /// <summary>
    /// Writes one fake PBP output for the staged workspace.
    /// </summary>
    /// <param name="workspace">Prepared workspace describing the native build inputs and outputs.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the simulated build.</param>
    public void Build(PspBuildWorkspace workspace, CancellationToken cancellationToken) {
        if (workspace == null) {
            throw new ArgumentNullException(nameof(workspace));
        }

        cancellationToken.ThrowIfCancellationRequested();
        LastWorkspace = workspace;
        string nativePbpDirectoryPath = Path.GetDirectoryName(workspace.NativePbpPath) ?? throw new InvalidOperationException("Native PBP directory could not be resolved.");
        Directory.CreateDirectory(nativePbpDirectoryPath);
        File.WriteAllText(workspace.NativePbpPath, "fake pbp");
    }
}
