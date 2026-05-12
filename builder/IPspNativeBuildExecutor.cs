namespace helengine.psp.builder;

/// <summary>
/// Executes the native PSP build that produces the intermediate PBP from the staged generated-core inputs.
/// </summary>
public interface IPspNativeBuildExecutor {
    /// <summary>
    /// Executes the native PSP build for one prepared workspace.
    /// </summary>
    /// <param name="workspace">Prepared PSP build workspace.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the native build.</param>
    void Build(PspBuildWorkspace workspace, CancellationToken cancellationToken);
}
