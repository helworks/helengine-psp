namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies PSP-specific generated-core compatibility rewrites applied before the native build.
/// </summary>
public sealed class PspGeneratedCoreCompatibilityNormalizerTests {
    /// <summary>
    /// Ensures clean generated serializer sources stay untouched so PSP builds rely on engine-side codegen fixes instead of post-processing.
    /// </summary>
    [Fact]
    public void Normalize_whenEditorAssetBinarySerializerIsAlreadyClean_doesNotInjectObsoleteHelpers() {
        string generatedCoreRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(generatedCoreRoot);

        string headerPath = Path.Combine(generatedCoreRoot, "EditorAssetBinarySerializer.hpp");
        string sourcePath = Path.Combine(generatedCoreRoot, "EditorAssetBinarySerializer.cpp");
        string headerContents =
            "class EditorAssetBinarySerializer\n"
            + "{\n"
            + "    static ::SceneEntityPlatformAddedComponentAsset* ReadSceneEntityPlatformAddedComponentAsset(::EngineBinaryReader* reader, uint8_t sceneEntityPayloadVersion);\n"
            + "    static ::SceneEntityPlatformAddedComponentAsset* ReadSceneEntityPlatformAddedComponentAssetValue(::EngineBinaryReader* reader);\n"
            + "};\n";
        string sourceContents =
            "::SceneEntityPlatformAddedComponentAsset* EditorAssetBinarySerializer::ReadSceneEntityPlatformAddedComponentAssetValue(::EngineBinaryReader* reader)\n"
            + "{\n"
            + "return ReadSceneEntityPlatformAddedComponentAsset(reader, SceneEntityPayloadVersion);\n"
            + "}\n";
        File.WriteAllText(headerPath, headerContents);
        File.WriteAllText(sourcePath, sourceContents);

        new PspGeneratedCoreCompatibilityNormalizer().Normalize(generatedCoreRoot);

        Assert.Equal(headerContents, File.ReadAllText(headerPath));
        Assert.Equal(sourceContents, File.ReadAllText(sourcePath));
        Assert.DoesNotContain("ReadSceneEntityPlatformAddedComponentAssetCurrentVersion", File.ReadAllText(headerPath), StringComparison.Ordinal);
        Assert.DoesNotContain("WriteSceneComponentAssetRecordCurrentVersion", File.ReadAllText(sourcePath), StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures generated <c>EngineBinaryReader::ReadString()</c> null-string reads are rewritten without altering pointer-returning readers.
    /// </summary>
    [Fact]
    public void Normalize_whenEngineBinaryReaderContainsMixedNullableReaders_rewritesOnlyReadStringNullBranch() {
        string generatedCoreRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(generatedCoreRoot);

        string sourcePath = Path.Combine(generatedCoreRoot, "EngineBinaryReader.cpp");
        File.WriteAllText(
            sourcePath,
            "Array<T>* EngineBinaryReader::ReadArray(Func<::EngineBinaryReader*, T>* readElement)\n{\nconst int32_t length = this->ReadInt32();\n    if (length == -1)\n    {\nreturn nullptr;    }\nelse     if (length < -1)\n    {\nthrow new InvalidOperationException(\"Array length cannot be negative.\");\n    }\nreturn values;}\n\n"
            + "Array<uint8_t>* EngineBinaryReader::ReadByteArray()\n{\nconst int32_t length = this->ReadInt32();\n    if (length == -1)\n    {\nreturn nullptr;    }\nelse     if (length < -1)\n    {\nthrow new InvalidOperationException(\"Byte array length cannot be negative.\");\n    }\nreturn bytes;}\n\n"
            + "std::string EngineBinaryReader::ReadString()\n{\nconst int32_t length = this->ReadInt32();\n    if (length == -1)\n    {\nreturn nullptr;    }\nelse     if (length < -1)\n    {\nthrow new InvalidOperationException(\"String length cannot be negative.\");\n    }\nelse     if (length == 0)\n    {\nreturn String::Empty;    }\nreturn Encoding::GetString(Encoding::UTF8, bytes);}\n");

        new PspGeneratedCoreCompatibilityNormalizer().Normalize(generatedCoreRoot);

        string updatedSourceContents = File.ReadAllText(sourcePath);
        Assert.Contains("Array<T>* EngineBinaryReader::ReadArray", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("Array<uint8_t>* EngineBinaryReader::ReadByteArray", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("std::string EngineBinaryReader::ReadString()", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("Array length cannot be negative.", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("Byte array length cannot be negative.", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("String length cannot be negative.", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("Array<T>* EngineBinaryReader::ReadArray(Func<::EngineBinaryReader*, T>* readElement)\n{\nconst int32_t length = this->ReadInt32();\n    if (length == -1)\n    {\nreturn nullptr;    }", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("Array<uint8_t>* EngineBinaryReader::ReadByteArray()\n{\nconst int32_t length = this->ReadInt32();\n    if (length == -1)\n    {\nreturn nullptr;    }", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("std::string EngineBinaryReader::ReadString()\n{\nconst int32_t length = this->ReadInt32();\n    if (length == -1)\n    {\nreturn String::Empty;    }", updatedSourceContents, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures generated <c>Path::IsPathRooted()</c> recognizes console device roots such as <c>ms0:/</c> and <c>cdrom0:\</c>.
    /// </summary>
    [Fact]
    public void Normalize_whenGeneratedPathUsesFilesystemAbsoluteOnly_rewritesConsoleRootDetection() {
        string generatedCoreRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        string systemIoRoot = Path.Combine(generatedCoreRoot, "system", "io");
        Directory.CreateDirectory(systemIoRoot);

        string sourcePath = Path.Combine(systemIoRoot, "path.cpp");
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

        new PspGeneratedCoreCompatibilityNormalizer().Normalize(generatedCoreRoot);

        string updatedSourceContents = File.ReadAllText(sourcePath);
        Assert.Contains("#include <cctype>", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("bool PathLooksLikeConsoleRootedPath(const std::string& path)", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("return std::filesystem::path(path).is_absolute() || PathLooksLikeConsoleRootedPath(path);", updatedSourceContents, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures generated platform configuration no longer advertises the PS2 runtime when the PSP builder normalizes a PS2-flavored generated core.
    /// </summary>
    [Fact]
    public void Normalize_whenGeneratedConfigTargetsPs2_rewrites_platform_flags_for_psp_builds() {
        string generatedCoreRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(generatedCoreRoot);

        string configPath = Path.Combine(generatedCoreRoot, "helcpp_config.hpp");
        File.WriteAllText(
            configPath,
            "#pragma once\n\n"
            + "#define HE_CPP_GENERATED_CONFIG 1\n"
            + "#define HE_CPP_COMPILER_GCC 1\n"
            + "#define HE_CPP_PLATFORM_PS2 1\n"
            + "#define HE_CPP_PLATFORM_IS_WINDOWS_HOST 0\n");

        new PspGeneratedCoreCompatibilityNormalizer().Normalize(generatedCoreRoot);

        string updatedConfigContents = File.ReadAllText(configPath);
        Assert.Contains("#define HE_CPP_COMPILER_GCC 1", updatedConfigContents, StringComparison.Ordinal);
        Assert.Contains("#define HE_CPP_PLATFORM_PS2 0", updatedConfigContents, StringComparison.Ordinal);
        Assert.Contains("#define HE_CPP_PLATFORM_IS_WINDOWS_HOST 0", updatedConfigContents, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures generated <c>ContentManager::LoadProcessedContent()</c> closes opened file streams after processor reads complete.
    /// </summary>
    [Fact]
    public void Normalize_whenContentManagerReturnsProcessorReadDirectly_rewrites_stream_lifetime() {
        string generatedCoreRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(generatedCoreRoot);

        string sourcePath = Path.Combine(generatedCoreRoot, "ContentManager.cpp");
        File.WriteAllText(
            sourcePath,
            "template <typename T>\n"
            + "T ContentManager::LoadProcessedContent(std::string fullPath, ::IContentProcessor_1<T>* processor)\n"
            + "{\n"
            + "::FileStream *stream = File::OpenRead(fullPath);\n"
            + "return processor->Read(stream);}\n");

        new PspGeneratedCoreCompatibilityNormalizer().Normalize(generatedCoreRoot);

        string updatedSourceContents = File.ReadAllText(sourcePath);
        Assert.Contains("T content = processor->Read(stream);", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("delete stream;", updatedSourceContents, StringComparison.Ordinal);
        Assert.Contains("return content;}", updatedSourceContents, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures generated stream headers expose property-style accessors expected by transpiled gameplay code.
    /// </summary>
    [Fact]
    public void Normalize_whenGeneratedStreamHeaderUsesRuntimeMethodNames_adds_property_style_wrappers() {
        string generatedCoreRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        string systemIoRoot = Path.Combine(generatedCoreRoot, "system", "io");
        Directory.CreateDirectory(systemIoRoot);

        string headerPath = Path.Combine(systemIoRoot, "stream.hpp");
        File.WriteAllText(
            headerPath,
            "class Stream {\n"
            + "public:\n"
            + "    virtual size_t Length() const = 0;\n"
            + "    virtual size_t Position() const = 0;\n"
            + "    virtual void SetPosition(size_t value) = 0;\n"
            + "\n"
            + "    virtual bool CanTimeout() const { return false; }\n"
            + "};\n");

        new PspGeneratedCoreCompatibilityNormalizer().Normalize(generatedCoreRoot);

        string updatedHeaderContents = File.ReadAllText(headerPath);
        Assert.Contains("size_t get_Length() const { return Length(); }", updatedHeaderContents, StringComparison.Ordinal);
        Assert.Contains("size_t get_Position() const { return Position(); }", updatedHeaderContents, StringComparison.Ordinal);
        Assert.Contains("void set_Position(size_t value) { SetPosition(value); }", updatedHeaderContents, StringComparison.Ordinal);
    }
}
