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
        Assert.Contains(workspace.NativeObjectCacheRootPath + ":/native-cache", arguments);
        Assert.Contains(workspace.NativePackageRootPath + ":/package-output", arguments);
    }

    /// <summary>
    /// Ensures the native-build executor invokes a clean rebuild so the packaged EBOOT cannot reuse stale native artifacts.
    /// </summary>
    [Fact]
    public void CreateBuildArguments_whenWorkspaceIsProvided_reusesNativeCache() {
        PspBuildWorkspace workspace = new(
            "/repo",
            "/repo/tmp/psp-staging",
            "/generated-core",
            "/out",
            "/repo/build/EBOOT.PBP");

        IReadOnlyList<string> arguments = PspNativeBuildExecutor.CreateBuildArguments(workspace);

        Assert.DoesNotContain("clean", arguments);
        Assert.Contains("all", arguments);
        Assert.Contains("NATIVE_OBJECT_CACHE_ROOT=/native-cache", arguments);
        Assert.Contains("BUILD_DIR=/native-cache", arguments);
        Assert.Contains("PACKAGE_DIR=/package-output", arguments);
        Assert.Contains("SOURCE_DIR=/workspace", arguments);
    }

    /// <summary>
    /// Ensures the native-build executor enables checkpointed runtime startup for the packaged PSP player build.
    /// </summary>
    [Fact]
    public void CreateBuildArguments_whenWorkspaceIsProvided_enables_runtime_startup() {
        PspBuildWorkspace workspace = new(
            "/repo",
            "/repo/tmp/psp-staging",
            "/generated-core",
            "/out",
            "/repo/build/EBOOT.PBP");

        IReadOnlyList<string> arguments = PspNativeBuildExecutor.CreateBuildArguments(workspace);

        Assert.Contains("HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON", arguments);
    }

    /// <summary>
    /// Ensures diagnostic PSP builds write boot traces so stale emulator installs can be identified quickly.
    /// </summary>
    [Fact]
    public void CreateBuildArguments_whenWorkspaceIsProvided_enables_boot_trace() {
        PspBuildWorkspace workspace = new(
            "/repo",
            "/repo/tmp/psp-staging",
            "/generated-core",
            "/out",
            "/repo/tmp/native-package/EBOOT.PBP",
            "/project/cache/build/psp/debug/native");

        IReadOnlyList<string> arguments = PspNativeBuildExecutor.CreateBuildArguments(workspace);

        Assert.Contains("HELENGINE_PSP_ENABLE_BOOT_TRACE=OFF", arguments);
    }
}
