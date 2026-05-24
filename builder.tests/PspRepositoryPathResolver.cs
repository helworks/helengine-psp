namespace helengine.psp.builder.tests;

/// <summary>
/// Resolves the PSP repository root for tests that inspect source files directly from the working tree.
/// </summary>
public static class PspRepositoryPathResolver {
    /// <summary>
    /// Returns the absolute PSP repository root discovered from the environment or the current test assembly location.
    /// </summary>
    /// <returns>Absolute PSP repository root path.</returns>
    public static string ResolveRepositoryRootPath() {
        string configuredRepositoryRootPath = Environment.GetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT") ?? string.Empty;
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

        throw new InvalidOperationException("Could not resolve the helengine-psp repository root for source-inspection tests.");
    }

    /// <summary>
    /// Returns whether one path looks like the PSP repository root.
    /// </summary>
    /// <param name="path">Candidate repository root path.</param>
    /// <returns>True when the expected builder and native-runtime entrypoint files exist.</returns>
    static bool IsRepositoryRootPath(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            return false;
        }

        string builderProjectPath = Path.Combine(path, "builder", "helengine.psp.builder.csproj");
        string nativeEntrypointPath = Path.Combine(path, "src", "platform", "psp", "PspBootHost.cpp");
        return File.Exists(builderProjectPath) && File.Exists(nativeEntrypointPath);
    }
}
