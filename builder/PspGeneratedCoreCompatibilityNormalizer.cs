using System.Text.RegularExpressions;

namespace helengine.psp.builder;

/// <summary>
/// Applies PSP-specific compatibility rewrites to regenerated native core sources before the Docker build compiles them.
/// </summary>
public sealed class PspGeneratedCoreCompatibilityNormalizer {
    /// <summary>
    /// Rewrites generated serializer sources that still contain unsupported inline callback syntax.
    /// </summary>
    /// <param name="generatedCoreRootPath">Absolute generated-core source root produced by the editor build graph.</param>
    public void Normalize(string generatedCoreRootPath) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }

        NormalizePlatformConfiguration(generatedCoreRootPath);
        NormalizeEditorAssetBinarySerializer(generatedCoreRootPath);
        NormalizeEngineBinaryReader(generatedCoreRootPath);
        NormalizeContentManager(generatedCoreRootPath);
        NormalizePathRootDetection(generatedCoreRootPath);
    }

    /// <summary>
    /// Rewrites generated platform configuration so PSP native builds do not compile PS2-only runtime branches.
    /// </summary>
    /// <param name="generatedCoreRootPath">Absolute generated-core source root produced by the editor build graph.</param>
    static void NormalizePlatformConfiguration(string generatedCoreRootPath) {
        string headerPath = Path.Combine(generatedCoreRootPath, "helcpp_config.hpp");
        if (!File.Exists(headerPath)) {
            return;
        }

        string headerContents = File.ReadAllText(headerPath);
        string updatedHeaderContents = headerContents.Replace(
            "#define HE_CPP_PLATFORM_PS2 1",
            "#define HE_CPP_PLATFORM_PS2 0",
            StringComparison.Ordinal);

        if (!string.Equals(headerContents, updatedHeaderContents, StringComparison.Ordinal)) {
            File.WriteAllText(headerPath, updatedHeaderContents);
        }
    }

    /// <summary>
    /// Rewrites malformed callback expressions inside the generated scene serializer so PSP native compilation can succeed.
    /// </summary>
    /// <param name="generatedCoreRootPath">Absolute generated-core source root produced by the editor build graph.</param>
    static void NormalizeEditorAssetBinarySerializer(string generatedCoreRootPath) {
        string headerPath = Path.Combine(generatedCoreRootPath, "EditorAssetBinarySerializer.hpp");
        string sourcePath = Path.Combine(generatedCoreRootPath, "EditorAssetBinarySerializer.cpp");
        if (!File.Exists(headerPath) || !File.Exists(sourcePath)) {
            return;
        }

        string headerContents = File.ReadAllText(headerPath);
        string sourceContents = File.ReadAllText(sourcePath);
        string newline = sourceContents.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";

        string updatedHeaderContents = headerContents;
        string addedComponentHelperDeclaration = "    static ::SceneEntityPlatformAddedComponentAsset* ReadSceneEntityPlatformAddedComponentAssetCurrentVersion(::EngineBinaryReader* reader);";
        if (!updatedHeaderContents.Contains(addedComponentHelperDeclaration, StringComparison.Ordinal)) {
            updatedHeaderContents = updatedHeaderContents.Replace(
                "    static ::SceneEntityPlatformAddedComponentAsset* ReadSceneEntityPlatformAddedComponentAsset(::EngineBinaryReader* reader, uint8_t sceneEntityPayloadVersion);",
                "    static ::SceneEntityPlatformAddedComponentAsset* ReadSceneEntityPlatformAddedComponentAsset(::EngineBinaryReader* reader, uint8_t sceneEntityPayloadVersion);" + newline + newline + addedComponentHelperDeclaration,
                StringComparison.Ordinal);
        }

        string sceneRecordWriterHelperDeclaration = "    static void WriteSceneComponentAssetRecordCurrentVersion(::EngineBinaryWriter* writer, ::SceneComponentAssetRecord* record);";
        if (!updatedHeaderContents.Contains(sceneRecordWriterHelperDeclaration, StringComparison.Ordinal)) {
            updatedHeaderContents = updatedHeaderContents.Replace(
                "    static void WriteSceneComponentAssetRecord(::EngineBinaryWriter* writer, ::SceneComponentAssetRecord* record, uint8_t sceneEntityPayloadVersion);",
                "    static void WriteSceneComponentAssetRecord(::EngineBinaryWriter* writer, ::SceneComponentAssetRecord* record, uint8_t sceneEntityPayloadVersion);" + newline + newline + sceneRecordWriterHelperDeclaration,
                StringComparison.Ordinal);
        }

        string updatedSourceContents = sourceContents
            .Replace(
                "new Func<EngineBinaryReader*, std::string>(valueReader => valueReader->ReadString())",
                "new Func<EngineBinaryReader*, std::string>(&EditorAssetBinarySerializer::ReadStringValue)",
                StringComparison.Ordinal)
            .Replace(
                "new Func<EngineBinaryReader*, SceneEntityPlatformAddedComponentAsset*>(valueReader => ReadSceneEntityPlatformAddedComponentAsset(valueReader, SceneEntityPayloadVersion))",
                "new Func<EngineBinaryReader*, SceneEntityPlatformAddedComponentAsset*>(&EditorAssetBinarySerializer::ReadSceneEntityPlatformAddedComponentAssetCurrentVersion)",
                StringComparison.Ordinal);

        updatedSourceContents = Regex.Replace(
            updatedSourceContents,
            @"new Action<EngineBinaryWriter\*, SceneComponentAssetRecord\*>\(\(valueWriter, value\) => WriteSceneComponentAssetRecord\(valueWriter, value, SceneEntityPayloadVersion\);\s*\)\)",
            "new Action<EngineBinaryWriter*, SceneComponentAssetRecord*>(&EditorAssetBinarySerializer::WriteSceneComponentAssetRecordCurrentVersion))",
            RegexOptions.CultureInvariant);
        updatedSourceContents = Regex.Replace(
            updatedSourceContents,
            @"new Action<EngineBinaryWriter\*, std::string>\(\(valueWriter, value\) => valueWriter->WriteString\(value\);\s*\)\)",
            "new Action<EngineBinaryWriter*, std::string>(&EditorAssetBinarySerializer::WriteStringValue))",
            RegexOptions.CultureInvariant);

        string addedComponentReaderHelperDefinition = "::SceneEntityPlatformAddedComponentAsset* EditorAssetBinarySerializer::ReadSceneEntityPlatformAddedComponentAssetCurrentVersion(::EngineBinaryReader* reader)" + newline
            + "{" + newline
            + "return ReadSceneEntityPlatformAddedComponentAsset(reader, SceneEntityPayloadVersion);" + newline
            + "}";
        string sceneRecordWriterHelperDefinition = "void EditorAssetBinarySerializer::WriteSceneComponentAssetRecordCurrentVersion(::EngineBinaryWriter* writer, ::SceneComponentAssetRecord* record)" + newline
            + "{" + newline
            + "WriteSceneComponentAssetRecord(writer, record, SceneEntityPayloadVersion);" + newline
            + "}";
        if (!updatedSourceContents.Contains("::SceneEntityPlatformAddedComponentAsset* EditorAssetBinarySerializer::ReadSceneEntityPlatformAddedComponentAssetCurrentVersion(", StringComparison.Ordinal)) {
            updatedSourceContents += newline + newline + addedComponentReaderHelperDefinition;
        }
        if (!updatedSourceContents.Contains("void EditorAssetBinarySerializer::WriteSceneComponentAssetRecordCurrentVersion(", StringComparison.Ordinal)) {
            updatedSourceContents += newline + newline + sceneRecordWriterHelperDefinition;
        }

        if (!string.Equals(headerContents, updatedHeaderContents, StringComparison.Ordinal)) {
            File.WriteAllText(headerPath, updatedHeaderContents);
        }
        if (!string.Equals(sourceContents, updatedSourceContents, StringComparison.Ordinal)) {
            File.WriteAllText(sourcePath, updatedSourceContents);
        }
    }

    /// <summary>
    /// Rewrites generated nullable-string deserialization to avoid constructing <see cref="string"/> from a null pointer on PSP.
    /// </summary>
    /// <param name="generatedCoreRootPath">Absolute generated-core source root produced by the editor build graph.</param>
    static void NormalizeEngineBinaryReader(string generatedCoreRootPath) {
        string sourcePath = Path.Combine(generatedCoreRootPath, "EngineBinaryReader.cpp");
        if (!File.Exists(sourcePath)) {
            return;
        }

        string sourceContents = File.ReadAllText(sourcePath);
        string updatedSourceContents = Regex.Replace(
            sourceContents,
            @"(std::string EngineBinaryReader::ReadString\(\)\s*\{\s*const int32_t length = this->ReadInt32\(\);\s*if \(length == -1\)\s*\{\s*)return nullptr;(\s*\}\s*else\s+if \(length < -1\))",
            "$1return String::Empty;$2",
            RegexOptions.CultureInvariant);

        if (!string.Equals(sourceContents, updatedSourceContents, StringComparison.Ordinal)) {
            File.WriteAllText(sourcePath, updatedSourceContents);
        }
    }

    /// <summary>
    /// Rewrites generated content loading so native PSP builds always close opened file streams after processor reads complete.
    /// </summary>
    /// <param name="generatedCoreRootPath">Absolute generated-core source root produced by the editor build graph.</param>
    static void NormalizeContentManager(string generatedCoreRootPath) {
        string sourcePath = Path.Combine(generatedCoreRootPath, "ContentManager.cpp");
        if (!File.Exists(sourcePath)) {
            return;
        }

        string sourceContents = File.ReadAllText(sourcePath);
        string newline = sourceContents.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
        string updatedSourceContents = sourceContents.Replace(
            "{\n::FileStream *stream = File::OpenRead(fullPath);\nreturn processor->Read(stream);}",
            "{"
            + newline + "::FileStream *stream = File::OpenRead(fullPath);"
            + newline + "T content = processor->Read(stream);"
            + newline + "delete stream;"
            + newline + "return content;}",
            StringComparison.Ordinal);

        if (!string.Equals(sourceContents, updatedSourceContents, StringComparison.Ordinal)) {
            File.WriteAllText(sourcePath, updatedSourceContents);
        }
    }

    /// <summary>
    /// Rewrites generated path-root detection so console device paths such as <c>ms0:/</c> and <c>cdrom0:\</c> stay rooted on native console hosts.
    /// </summary>
    /// <param name="generatedCoreRootPath">Absolute generated-core source root produced by the editor build graph.</param>
    static void NormalizePathRootDetection(string generatedCoreRootPath) {
        string[] sourcePaths = Directory.GetFiles(generatedCoreRootPath, "path.cpp", SearchOption.AllDirectories);
        for (int index = 0; index < sourcePaths.Length; index++) {
            string sourcePath = sourcePaths[index];
            string relativePath = Path.GetRelativePath(generatedCoreRootPath, sourcePath).Replace('\\', '/');
            if (!relativePath.EndsWith("system/io/path.cpp", StringComparison.OrdinalIgnoreCase)) {
                continue;
            }

            string sourceContents = File.ReadAllText(sourcePath);
            string newline = sourceContents.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
            string updatedSourceContents = sourceContents;
            if (!updatedSourceContents.Contains("#include <cctype>", StringComparison.Ordinal)) {
                updatedSourceContents = updatedSourceContents.Replace(
                    "#include <filesystem>",
                    "#include <filesystem>" + newline + "#include <cctype>",
                    StringComparison.Ordinal);
            }

            const string helperName = "PathLooksLikeConsoleRootedPath";
            string helperDefinition =
                "namespace {" + newline
                + "bool " + helperName + "(const std::string& path) {" + newline
                + "    std::size_t colonIndex = path.find(':');" + newline
                + "    if (colonIndex == std::string::npos || colonIndex == 0 || colonIndex + 1 >= path.size()) {" + newline
                + "        return false;" + newline
                + "    }" + newline + newline
                + "    char separator = path[colonIndex + 1];" + newline
                + "    if (separator != '/' && separator != '\\\\') {" + newline
                + "        return false;" + newline
                + "    }" + newline + newline
                + "    for (std::size_t pathIndex = 0; pathIndex < colonIndex; pathIndex++) {" + newline
                + "        if (std::isalnum(static_cast<unsigned char>(path[pathIndex])) == 0) {" + newline
                + "            return false;" + newline
                + "        }" + newline
                + "    }" + newline + newline
                + "    return true;" + newline
                + "}" + newline
                + "}";
            if (!updatedSourceContents.Contains("bool " + helperName + "(const std::string& path)", StringComparison.Ordinal)) {
                updatedSourceContents = updatedSourceContents.Replace(
                    "bool Path::IsPathRooted(const std::string& path) {",
                    helperDefinition + newline + newline + "bool Path::IsPathRooted(const std::string& path) {",
                    StringComparison.Ordinal);
            }

            updatedSourceContents = updatedSourceContents.Replace(
                "return std::filesystem::path(path).is_absolute();",
                "return std::filesystem::path(path).is_absolute() || PathLooksLikeConsoleRootedPath(path);",
                StringComparison.Ordinal);

            if (!string.Equals(sourceContents, updatedSourceContents, StringComparison.Ordinal)) {
                File.WriteAllText(sourcePath, updatedSourceContents);
            }
        }
    }
}
