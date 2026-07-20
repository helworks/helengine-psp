using System.Diagnostics;
using helengine.baseplatform.Builders;

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
        NativeProcessRunResult result = new NativeProcessRunner().Run(startInfo, cancellationToken);
        if (result.ExitCode != 0) {
            throw new InvalidOperationException(
                $"The PSP native build failed with exit code {result.ExitCode}."
                + Environment.NewLine
                + result.StandardOutput
                + Environment.NewLine
                + result.StandardError);
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
            "HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON",
            "HELENGINE_PSP_ENABLE_BOOT_TRACE=ON"
        ];
    }
}
