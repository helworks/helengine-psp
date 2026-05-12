using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.psp.builder.tests;

/// <summary>
/// Collects streamed platform build diagnostics for assertions.
/// </summary>
public sealed class RecordingDiagnosticReporter : IPlatformBuildDiagnosticReporter {
    /// <summary>
    /// Gets the diagnostics reported during the current test.
    /// </summary>
    public List<PlatformBuildDiagnostic> Diagnostics { get; } = [];

    /// <summary>
    /// Records one diagnostic for later assertions.
    /// </summary>
    /// <param name="diagnostic">Diagnostic emitted by the builder.</param>
    public void Report(PlatformBuildDiagnostic diagnostic) {
        Diagnostics.Add(diagnostic);
    }
}
