using helengine.psp.builder;

/// <summary>
/// Exposes the minimal PSP builder command-line surface used by editor discovery and smoke tests.
/// </summary>
public static class Program {
    /// <summary>
    /// Runs the minimal PSP builder command-line surface used by tests and editor discovery.
    /// </summary>
    /// <param name="args">Command-line arguments.</param>
    /// <returns>Process exit code.</returns>
    public static int Main(string[] args) {
        if (args.Length > 0 && string.Equals(args[0], "--describe", StringComparison.OrdinalIgnoreCase)) {
            PspPlatformAssetBuilder builder = new();
            Console.WriteLine(builder.Descriptor.BuilderId);
            Console.WriteLine(builder.Descriptor.TargetPlatformId);
            Console.WriteLine(builder.Definition.DisplayName);
            Console.WriteLine(builder.Definition.BuildProfiles.Length);
            Console.WriteLine(builder.Definition.GraphicsProfiles.Length);
            return 0;
        }

        if (args.Length > 0 && string.Equals(args[0], "--normalize-generated-core", StringComparison.OrdinalIgnoreCase)) {
            return NormalizeGeneratedCore(args);
        }

        if (args.Length > 0 && string.Equals(args[0], "--smoke-test", StringComparison.OrdinalIgnoreCase)) {
            PspPlatformAssetBuilder builder = new();
            Console.WriteLine(builder.Descriptor.BuilderId);
            string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-psp-builder-smoke-" + Guid.NewGuid().ToString("N"));
            string generatedCoreRootPath = Path.Combine(workingRoot, "generated-core");
            string runtimeRootPath = Path.Combine(generatedCoreRootPath, "runtime");
            Directory.CreateDirectory(runtimeRootPath);
            File.WriteAllText(Path.Combine(generatedCoreRootPath, "helengine_core_amalgamated.cpp"), "// generated");
            File.WriteAllText(Path.Combine(generatedCoreRootPath, "helcpp_config.hpp"), "#pragma once");
            File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_startup_manifest.cpp"), "// startup");
            File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_scene_catalog_manifest.cpp"), "// scene catalog");
            Console.WriteLine("Smoke test prepared generated-core prerequisites.");
            Console.WriteLine(generatedCoreRootPath);
            Console.WriteLine("ok");
            return 0;
        }

        Console.WriteLine("helengine.psp.builder --describe|--smoke-test|--normalize-generated-core <generated-core-root>");
        return 0;
    }

    /// <summary>
    /// Normalizes one generated-core tree in place so direct PSP native builds use the same compatibility rewrites as the editor-owned builder path.
    /// </summary>
    /// <param name="args">Command-line arguments.</param>
    /// <returns>Process exit code.</returns>
    static int NormalizeGeneratedCore(string[] args) {
        if (args.Length < 2 || string.IsNullOrWhiteSpace(args[1])) {
            Console.Error.WriteLine("Missing generated-core root path.");
            return 1;
        }

        string generatedCoreRootPath = args[1];
        if (!Directory.Exists(generatedCoreRootPath)) {
            Console.Error.WriteLine($"Generated-core root was not found: {generatedCoreRootPath}");
            return 1;
        }

        new PspGeneratedCoreCompatibilityNormalizer().Normalize(generatedCoreRootPath);
        Console.WriteLine($"Normalized generated-core compatibility rewrites under: {generatedCoreRootPath}");
        return 0;
    }
}
