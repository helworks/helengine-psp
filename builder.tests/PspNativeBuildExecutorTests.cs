namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the Docker command shape used by the PSP native-build executor.
/// </summary>
public sealed class PspNativeBuildExecutorTests {
    /// <summary>
    /// Ensures the native-build executor mounts the repository and generated core roots and forwards the generated-core path to make.
    /// </summary>
    [Fact]
    public void CreateBuildArguments_whenWorkspaceIsProvided_mountsRepositoryAndGeneratedCore() {
        PspBuildWorkspace workspace = new(
            "/repo",
            "/repo/tmp/psp-staging",
            "/generated-core",
            "/out",
            "/repo/build/EBOOT.PBP");

        IReadOnlyList<string> arguments = PspNativeBuildExecutor.CreateBuildArguments(workspace);

        Assert.Equal("run", arguments[0]);
        Assert.Contains("-v", arguments);
        Assert.Contains($"{workspace.RepositoryRootPath}:/workspace", arguments);
        Assert.Contains($"{workspace.GeneratedCoreRootPath}:/generated-core", arguments);
        Assert.Contains("helengine-psp", arguments);
        Assert.Contains("make", arguments);
        Assert.Contains("HELENGINE_CORE_CPP_ROOT=/generated-core", arguments);
    }

    /// <summary>
    /// Ensures the native-build executor enables isolated boot explicitly so the PSP player can stay on a blank frame during runtime bring-up.
    /// </summary>
    [Fact]
    public void CreateBuildArguments_whenWorkspaceIsProvided_enables_isolated_boot() {
        PspBuildWorkspace workspace = new(
            "/repo",
            "/repo/tmp/psp-staging",
            "/generated-core",
            "/out",
            "/repo/build/EBOOT.PBP");

        IReadOnlyList<string> arguments = PspNativeBuildExecutor.CreateBuildArguments(workspace);

        Assert.Contains("HELENGINE_PSP_ISOLATED_BOOT=ON", arguments);
    }

    /// <summary>
    /// Ensures the native-build executor keeps checkpointed runtime startup disabled unless a bring-up build opts in explicitly.
    /// </summary>
    [Fact]
    public void CreateBuildArguments_whenWorkspaceIsProvided_disables_runtime_startup_by_default() {
        PspBuildWorkspace workspace = new(
            "/repo",
            "/repo/tmp/psp-staging",
            "/generated-core",
            "/out",
            "/repo/build/EBOOT.PBP");

        IReadOnlyList<string> arguments = PspNativeBuildExecutor.CreateBuildArguments(workspace);

        Assert.Contains("HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF", arguments);
    }
}
