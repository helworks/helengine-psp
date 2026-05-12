using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.psp.builder.tests;

/// <summary>
/// Collects streamed platform build progress updates for assertions.
/// </summary>
public sealed class RecordingProgressReporter : IPlatformBuildProgressReporter {
    /// <summary>
    /// Gets the progress updates reported during the current test.
    /// </summary>
    public List<PlatformBuildProgressUpdate> Updates { get; } = [];

    /// <summary>
    /// Records one progress update for later assertions.
    /// </summary>
    /// <param name="update">Progress update emitted by the builder.</param>
    public void Report(PlatformBuildProgressUpdate update) {
        Updates.Add(update);
    }
}
