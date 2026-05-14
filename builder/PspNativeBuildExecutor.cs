using System.Diagnostics;

namespace helengine.psp.builder;

/// <summary>
/// Invokes the Docker-based native PSP build using the staged generated-core root.
/// </summary>
public sealed class PspNativeBuildExecutor : IPspNativeBuildExecutor {
    /// <summary>
    /// Executes the native PSP build for one prepared workspace.
    /// </summary>
    /// <param name="workspace">Prepared PSP build workspace.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the native build.</param>
    public void Build(PspBuildWorkspace workspace, CancellationToken cancellationToken) {
        if (workspace == null) {
            throw new ArgumentNullException(nameof(workspace));
        }

        ProcessStartInfo startInfo = CreateStartInfo(workspace);
        using Process process = Process.Start(startInfo) ?? throw new InvalidOperationException("Failed to start the PSP native build.");
        using CancellationTokenRegistration cancellationRegistration = cancellationToken.Register(() => TryKillProcess(process));

        process.WaitForExit();
        cancellationToken.ThrowIfCancellationRequested();
        if (process.ExitCode != 0) {
            throw new InvalidOperationException($"The PSP native build failed with exit code {process.ExitCode}.");
        }
    }

    /// <summary>
    /// Creates the Docker process start-info for one prepared workspace.
    /// </summary>
    /// <param name="workspace">Prepared PSP build workspace.</param>
    /// <returns>Docker process start-info for the native PSP build.</returns>
    public static ProcessStartInfo CreateStartInfo(PspBuildWorkspace workspace) {
        if (workspace == null) {
            throw new ArgumentNullException(nameof(workspace));
        }

        ProcessStartInfo startInfo = new ProcessStartInfo {
            FileName = "docker",
            UseShellExecute = false,
            WorkingDirectory = workspace.RepositoryRootPath
        };

        IReadOnlyList<string> arguments = CreateBuildArguments(workspace);
        for (int index = 0; index < arguments.Count; index++) {
            startInfo.ArgumentList.Add(arguments[index]);
        }

        return startInfo;
    }

    /// <summary>
    /// Creates the Docker command arguments for one prepared workspace.
    /// </summary>
    /// <param name="workspace">Prepared PSP build workspace.</param>
    /// <returns>Ordered Docker command arguments.</returns>
    public static IReadOnlyList<string> CreateBuildArguments(PspBuildWorkspace workspace) {
        if (workspace == null) {
            throw new ArgumentNullException(nameof(workspace));
        }

        return [
            "run",
            "--rm",
            "-v",
            $"{workspace.RepositoryRootPath}:/workspace",
            "-v",
            $"{workspace.GeneratedCoreRootPath}:/generated-core",
            "-w",
            "/workspace",
            "helengine-psp",
            "make",
            "clean",
            "all",
            "HELENGINE_CORE_CPP_ROOT=/generated-core",
            "HELENGINE_PSP_ISOLATED_BOOT=ON",
            "HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON"
        ];
    }

    /// <summary>
    /// Attempts to stop one running native-build process when cancellation is requested.
    /// </summary>
    /// <param name="process">Running native-build process.</param>
    static void TryKillProcess(Process process) {
        if (process == null) {
            return;
        }

        try {
            if (!process.HasExited) {
                process.Kill(entireProcessTree: true);
            }
        } catch {
        }
    }
}
