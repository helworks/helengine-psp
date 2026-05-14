using System.Text;
using helengine.baseplatform.Manifest;

namespace helengine.psp.builder;

/// <summary>
/// Writes generated C++ runtime manifest source files consumed by the PSP native player.
/// </summary>
public sealed class PspRuntimeNativeManifestWriter {
    /// <summary>
    /// Writes the PSP startup and scene-catalog runtime manifests into the generated-core runtime folder.
    /// </summary>
    /// <param name="generatedCoreRootPath">Absolute generated-core root path.</param>
    /// <param name="manifest">Resolved build manifest whose runtime metadata should be embedded.</param>
    public void Write(string generatedCoreRootPath, PlatformBuildManifest manifest) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        } else if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        }

        string runtimeRootPath = Path.Combine(generatedCoreRootPath, "runtime");
        Directory.CreateDirectory(runtimeRootPath);

        File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_startup_manifest.hpp"), BuildStartupManifestHeaderContents());
        File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_startup_manifest.cpp"), BuildStartupManifestSourceContents(manifest));
        File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_scene_catalog_manifest.hpp"), BuildSceneCatalogManifestHeaderContents());
        File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_scene_catalog_manifest.cpp"), BuildSceneCatalogManifestSourceContents(manifest));
    }

    /// <summary>
    /// Builds the generated runtime startup manifest header.
    /// </summary>
    /// <returns>Generated C++ header text.</returns>
    static string BuildStartupManifestHeaderContents() {
        StringBuilder builder = new();
        builder.AppendLine("#pragma once");
        builder.AppendLine();
        builder.AppendLine("const char* he_get_runtime_startup_scene_relative_path();");
        builder.AppendLine("const char* he_get_runtime_platform_name();");
        builder.AppendLine("const char* he_get_runtime_platform_version();");
        return builder.ToString();
    }

    /// <summary>
    /// Builds the generated runtime scene-catalog manifest header.
    /// </summary>
    /// <returns>Generated C++ header text.</returns>
    static string BuildSceneCatalogManifestHeaderContents() {
        StringBuilder builder = new();
        builder.AppendLine("#pragma once");
        builder.AppendLine();
        builder.AppendLine("#include <cstddef>");
        builder.AppendLine();
        builder.AppendLine("struct HERuntimeSceneCatalogEntry {");
        builder.AppendLine("    const char* SceneId;");
        builder.AppendLine("    const char* CookedRelativePath;");
        builder.AppendLine("};");
        builder.AppendLine();
        builder.AppendLine("const HERuntimeSceneCatalogEntry* he_runtime_scene_catalog_entries(std::size_t* count);");
        builder.AppendLine("const char* he_runtime_scene_cooked_relative_path(const char* sceneId);");
        return builder.ToString();
    }

    /// <summary>
    /// Builds the generated runtime startup manifest implementation.
    /// </summary>
    /// <param name="manifest">Build manifest whose platform metadata should be embedded into native source.</param>
    /// <returns>Generated C++ implementation text.</returns>
    static string BuildStartupManifestSourceContents(PlatformBuildManifest manifest) {
        if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        }

        string startupSceneRelativePath = ResolveStartupSceneRelativePath(manifest);
        StringBuilder builder = new();
        builder.AppendLine("#include \"runtime/runtime_startup_manifest.hpp\"");
        builder.AppendLine();
        builder.AppendLine("static const char kRuntimeStartupSceneRelativePath[] = \"" + EscapeCppStringLiteral(startupSceneRelativePath) + "\";");
        builder.AppendLine("static const char kRuntimePlatformName[] = \"" + EscapeCppStringLiteral(manifest.PlatformName) + "\";");
        builder.AppendLine("static const char kRuntimePlatformVersion[] = \"" + EscapeCppStringLiteral(manifest.PlatformVersion) + "\";");
        builder.AppendLine();
        builder.AppendLine("const char* he_get_runtime_startup_scene_relative_path() {");
        builder.AppendLine("    return kRuntimeStartupSceneRelativePath;");
        builder.AppendLine("}");
        builder.AppendLine();
        builder.AppendLine("const char* he_get_runtime_platform_name() {");
        builder.AppendLine("    return kRuntimePlatformName;");
        builder.AppendLine("}");
        builder.AppendLine();
        builder.AppendLine("const char* he_get_runtime_platform_version() {");
        builder.AppendLine("    return kRuntimePlatformVersion;");
        builder.AppendLine("}");
        return builder.ToString();
    }

    /// <summary>
    /// Builds the generated runtime scene-catalog manifest implementation.
    /// </summary>
    /// <param name="manifest">Build manifest whose scene layout should be embedded into native source.</param>
    /// <returns>Generated C++ implementation text.</returns>
    static string BuildSceneCatalogManifestSourceContents(PlatformBuildManifest manifest) {
        if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        } else if (manifest.Scenes == null || manifest.Scenes.Length == 0) {
            throw new InvalidOperationException("Build manifest did not define any scenes.");
        }

        StringBuilder builder = new();
        builder.AppendLine("#include \"runtime/runtime_scene_catalog_manifest.hpp\"");
        builder.AppendLine();
        builder.AppendLine("#include <cstring>");
        builder.AppendLine("#include <stdexcept>");
        builder.AppendLine();
        builder.AppendLine("static const HERuntimeSceneCatalogEntry kRuntimeSceneCatalogEntries[] = {");
        for (int index = 0; index < manifest.Scenes.Length; index++) {
            PlatformBuildScene scene = manifest.Scenes[index];
            string cookedRelativePath = ResolveCookedRelativePath(scene);
            builder.Append("    { \"");
            builder.Append(EscapeCppStringLiteral(scene.SceneId));
            builder.Append("\", \"");
            builder.Append(EscapeCppStringLiteral(cookedRelativePath));
            builder.AppendLine("\" },");
        }

        builder.AppendLine("};");
        builder.AppendLine("static const std::size_t kRuntimeSceneCatalogEntryCount = sizeof(kRuntimeSceneCatalogEntries) / sizeof(kRuntimeSceneCatalogEntries[0]);");
        builder.AppendLine();
        builder.AppendLine("const HERuntimeSceneCatalogEntry* he_runtime_scene_catalog_entries(std::size_t* count) {");
        builder.AppendLine("    if (count != nullptr) {");
        builder.AppendLine("        *count = kRuntimeSceneCatalogEntryCount;");
        builder.AppendLine("    }");
        builder.AppendLine();
        builder.AppendLine("    return kRuntimeSceneCatalogEntries;");
        builder.AppendLine("}");
        builder.AppendLine();
        builder.AppendLine("const char* he_runtime_scene_cooked_relative_path(const char* sceneId) {");
        builder.AppendLine("    if (sceneId == nullptr || sceneId[0] == '\\0') {");
        builder.AppendLine("        throw std::invalid_argument(\"Runtime scene id is required.\");");
        builder.AppendLine("    }");
        builder.AppendLine();
        builder.AppendLine("    for (std::size_t index = 0; index < kRuntimeSceneCatalogEntryCount; index++) {");
        builder.AppendLine("        const HERuntimeSceneCatalogEntry& entry = kRuntimeSceneCatalogEntries[index];");
        builder.AppendLine("        if (std::strcmp(entry.SceneId, sceneId) == 0) {");
        builder.AppendLine("            return entry.CookedRelativePath;");
        builder.AppendLine("        }");
        builder.AppendLine("    }");
        builder.AppendLine();
        builder.AppendLine("    throw std::runtime_error(\"Runtime scene id was not found in the scene catalog manifest.\");");
        builder.AppendLine("}");
        return builder.ToString();
    }

    /// <summary>
    /// Resolves the cooked runtime scene path embedded into the generated startup source.
    /// </summary>
    /// <param name="manifest">Final build manifest that carries the startup scene metadata.</param>
    /// <returns>Runtime-relative cooked scene path.</returns>
    static string ResolveStartupSceneRelativePath(PlatformBuildManifest manifest) {
        if (manifest.Scenes == null || manifest.Scenes.Length == 0) {
            throw new InvalidOperationException("Build manifest did not define any scenes.");
        }

        string startupSceneId = manifest.StartupSceneId;
        if (string.IsNullOrWhiteSpace(startupSceneId)) {
            startupSceneId = manifest.Scenes[0].SceneId;
        }

        for (int index = 0; index < manifest.Scenes.Length; index++) {
            PlatformBuildScene scene = manifest.Scenes[index];
            if (!string.Equals(scene.SceneId, startupSceneId, StringComparison.OrdinalIgnoreCase)) {
                continue;
            }

            if (scene.ResolvedMetadata != null) {
                for (int metadataIndex = 0; metadataIndex < scene.ResolvedMetadata.Length; metadataIndex++) {
                    KeyValuePair<string, string> entry = scene.ResolvedMetadata[metadataIndex];
                    if (string.Equals(entry.Key, "cooked-relative-path", StringComparison.OrdinalIgnoreCase)
                        && !string.IsNullOrWhiteSpace(entry.Value)) {
                        return entry.Value.Replace('\\', '/');
                    }
                }
            }

            PlatformBuildPayloadReference payloadReference = scene.PayloadReferences.FirstOrDefault();
            if (payloadReference != null && !string.IsNullOrWhiteSpace(payloadReference.SourceIdentity)) {
                return payloadReference.SourceIdentity.Replace('\\', '/');
            }

            throw new InvalidOperationException($"Startup scene '{scene.SceneId}' did not resolve a cooked-relative-path metadata entry.");
        }

        throw new InvalidOperationException($"Startup scene '{startupSceneId}' was not found in the build manifest.");
    }

    /// <summary>
    /// Resolves the cooked runtime scene path for one manifest scene.
    /// </summary>
    /// <param name="scene">Manifest scene whose cooked-relative path should be resolved.</param>
    /// <returns>Runtime-relative cooked scene path.</returns>
    static string ResolveCookedRelativePath(PlatformBuildScene scene) {
        if (scene == null) {
            throw new ArgumentNullException(nameof(scene));
        }

        if (scene.ResolvedMetadata != null) {
            for (int index = 0; index < scene.ResolvedMetadata.Length; index++) {
                KeyValuePair<string, string> entry = scene.ResolvedMetadata[index];
                if (string.Equals(entry.Key, "cooked-relative-path", StringComparison.OrdinalIgnoreCase)
                    && !string.IsNullOrWhiteSpace(entry.Value)) {
                    return entry.Value.Replace('\\', '/');
                }
            }
        }

        PlatformBuildPayloadReference payloadReference = scene.PayloadReferences.FirstOrDefault();
        if (payloadReference != null && !string.IsNullOrWhiteSpace(payloadReference.SourceIdentity)) {
            return payloadReference.SourceIdentity.Replace('\\', '/');
        }

        throw new InvalidOperationException($"Scene '{scene.SceneId}' did not resolve a cooked-relative-path metadata entry.");
    }

    /// <summary>
    /// Escapes one string for safe embedding inside a C++ string literal.
    /// </summary>
    /// <param name="value">String value to escape.</param>
    /// <returns>Escaped literal contents without the surrounding quotes.</returns>
    static string EscapeCppStringLiteral(string value) {
        if (string.IsNullOrEmpty(value)) {
            return string.Empty;
        }

        StringBuilder builder = new();
        for (int index = 0; index < value.Length; index++) {
            char current = value[index];
            if (current == '\\') {
                builder.Append("\\\\");
            } else if (current == '"') {
                builder.Append("\\\"");
            } else if (current == '\n') {
                builder.Append("\\n");
            } else if (current == '\r') {
                builder.Append("\\r");
            } else if (current == '\t') {
                builder.Append("\\t");
            } else {
                builder.Append(current);
            }
        }

        return builder.ToString();
    }
}
