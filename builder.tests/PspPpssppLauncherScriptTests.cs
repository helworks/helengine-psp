namespace helengine.psp.builder.tests;

/// <summary>
/// Guards the canonical PSP emulator launcher contract.
/// </summary>
public sealed class PspPpssppLauncherScriptTests {
    /// <summary>
    /// Ensures the canonical launcher requires one explicit artifact path, stages the full PSP app root into the PPSSPP memstick, and starts PPSSPP.
    /// </summary>
    [Fact]
    public void Launcher_RequiresArtifactPath_StagesFullAppRoot_AndStartsPpsspp() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string scriptPath = Path.Combine(repositoryRootPath, "scripts", "launch_in_emulator.ps1");

        Assert.True(File.Exists(scriptPath), "Expected scripts/launch_in_emulator.ps1 to exist.");

        string scriptSource = File.ReadAllText(scriptPath);

        Assert.Contains("[string]$ArtifactPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("PPSSPPWindows64.exe", scriptSource, StringComparison.Ordinal);
        Assert.Contains("EBOOT.PBP", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Copy-Item -LiteralPath $sourceRoot -Destination $targetRoot -Recurse -Force", scriptSource, StringComparison.Ordinal);
        Assert.Contains("PROCESS_ID=", scriptSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the root README documents the canonical launcher entrypoint.
    /// </summary>
    [Fact]
    public void Readme_DocumentsCanonicalLauncherWorkflow() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

        Assert.Contains("launch_in_emulator.ps1", readmeSource, StringComparison.Ordinal);
        Assert.Contains("-ArtifactPath", readmeSource, StringComparison.Ordinal);
        Assert.DoesNotContain("run_ppsspp_boot_check.ps1", readmeSource, StringComparison.Ordinal);
        Assert.DoesNotContain("install_psp_output_to_ppsspp.ps1", readmeSource, StringComparison.Ordinal);
    }
}
