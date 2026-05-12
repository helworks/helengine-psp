namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the PSP builder command-line entrypoints.
/// </summary>
public sealed class ProgramTests {
    /// <summary>
    /// Ensures the builder can normalize one generated-core tree in place for direct PSP native builds.
    /// </summary>
    [Fact]
    public void Main_whenNormalizeGeneratedCoreIsRequested_rewrites_console_path_root_detection() {
        string generatedCoreRootPath = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        string systemIoRootPath = Path.Combine(generatedCoreRootPath, "system", "io");
        Directory.CreateDirectory(systemIoRootPath);

        string sourcePath = Path.Combine(systemIoRootPath, "path.cpp");
        File.WriteAllText(
            sourcePath,
            "#include \"path.hpp\"\n\n"
            + "#include \"helcpp_config.hpp\"\n\n"
            + "#include <filesystem>\n\n"
            + "bool Path::IsPathRooted(const std::string& path) {\n"
            + "    if (path.empty()) {\n"
            + "        return false;\n"
            + "    }\n\n"
            + "    return std::filesystem::path(path).is_absolute();\n"
            + "}\n");

        int exitCode = global::Program.Main(["--normalize-generated-core", generatedCoreRootPath]);

        string updatedSourceContents = File.ReadAllText(sourcePath);
        Assert.Equal(0, exitCode);
        Assert.Contains("PathLooksLikeConsoleRootedPath", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("PathLooksLikeConsoleRootedPath(path)", updatedSourceContents, StringComparison.Ordinal);
    }
}
