using System.Reflection;
using helengine.editor;

namespace helengine.psp.builder;

/// <summary>
/// Imports source textures and source fonts through the shared editor import contracts for PSP builder-owned cook work items.
/// </summary>
public sealed class PspPlatformCookSourceProcessor : IPspPlatformCookSourceProcessor {
    /// <summary>
    /// Stable metadata suffix appended to cooked font asset ids when generating their atlas texture ids.
    /// </summary>
    const string FontAtlasSuffix = "#atlas";

    /// <summary>
    /// Relative Windows editor backend assembly path used when the importer assembly has not been loaded yet.
    /// </summary>
    const string EditorWindowsAssemblyRelativePath = @"engine\helengine.editor.windows\bin\Debug\net9.0-windows\helengine.editor.windows.dll";

    /// <summary>
    /// Relative editor host assembly path used when the importer-host assembly has not been loaded yet.
    /// </summary>
    const string EditorAppAssemblyRelativePath = @"helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll";

    /// <summary>
    /// Shared texture processor used to apply editor-resolved PSP texture settings.
    /// </summary>
    readonly TextureAssetProcessor TextureAssetProcessor;

    /// <summary>
    /// Lazily initialized default texture importer registrations loaded from the Windows editor backend.
    /// </summary>
    IReadOnlyList<TextureImporterRegistration> TextureImporterRegistrationsValue;

    /// <summary>
    /// Lazily initialized source font importer loaded from the Windows editor backend.
    /// </summary>
    IFontImporter FontImporterValue;

    /// <summary>
    /// Initializes the shared source processor used by PSP builder-owned work items.
    /// </summary>
    public PspPlatformCookSourceProcessor() {
        TextureAssetProcessor = new TextureAssetProcessor();
    }

    /// <summary>
    /// Imports one source texture asset and applies the resolved PSP texture processor settings.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked texture should preserve.</param>
    /// <param name="settings">Resolved PSP texture processor settings supplied by the editor.</param>
    /// <returns>Processed runtime texture payload ready for serialization.</returns>
    public TextureAsset CookTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source texture path must be provided.", nameof(sourceAssetPath));
        } else if (string.IsNullOrWhiteSpace(assetId)) {
            throw new ArgumentException("Texture asset id must be provided.", nameof(assetId));
        } else if (settings == null) {
            throw new ArgumentNullException(nameof(settings));
        } else if (!File.Exists(sourceAssetPath)) {
            throw new FileNotFoundException("Source texture file was not found.", sourceAssetPath);
        }

        TextureImporterRegistration importer = ResolveTextureImporter(sourceAssetPath);
        using FileStream stream = new FileStream(sourceAssetPath, FileMode.Open, FileAccess.Read, FileShare.Read);
        TextureAsset importedTextureAsset = importer.Importer.ImportTexture(stream);
        if (importedTextureAsset == null) {
            throw new InvalidOperationException($"Texture importer '{importer.ImporterId}' did not return a texture asset for '{sourceAssetPath}'.");
        }

        TextureAsset cookedTextureAsset = TextureAssetProcessor.Apply(importedTextureAsset, settings);
        cookedTextureAsset.Id = assetId;
        cookedTextureAsset.RuntimeAssetId = RuntimeAssetIdGenerator.Generate(assetId);
        return cookedTextureAsset;
    }

    /// <summary>
    /// Imports one source font atlas asset and applies the resolved PSP atlas texture processor settings.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source font path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked atlas texture should preserve.</param>
    /// <param name="settings">Resolved PSP texture processor settings supplied by the editor.</param>
    /// <returns>Processed runtime atlas texture payload ready for serialization.</returns>
    public TextureAsset CookFontAtlasTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source font path must be provided.", nameof(sourceAssetPath));
        } else if (string.IsNullOrWhiteSpace(assetId)) {
            throw new ArgumentException("Font atlas asset id must be provided.", nameof(assetId));
        } else if (settings == null) {
            throw new ArgumentNullException(nameof(settings));
        } else if (!File.Exists(sourceAssetPath)) {
            throw new FileNotFoundException("Source font file was not found.", sourceAssetPath);
        }

        TextureAsset importedAtlasTextureAsset = LoadSourceFontAtlasTexture(sourceAssetPath);
        TextureAsset cookedAtlasAsset = TextureAssetProcessor.Apply(importedAtlasTextureAsset, settings);
        cookedAtlasAsset.Id = assetId + FontAtlasSuffix;
        cookedAtlasAsset.RuntimeAssetId = RuntimeAssetIdGenerator.Generate(cookedAtlasAsset.Id);
        return cookedAtlasAsset;
    }

    /// <summary>
    /// Resolves the default texture importer registration for one source asset path.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path.</param>
    /// <returns>Matching default texture importer registration.</returns>
    TextureImporterRegistration ResolveTextureImporter(string sourceAssetPath) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source texture path must be provided.", nameof(sourceAssetPath));
        }

        string normalizedExtension = NormalizeExtension(Path.GetExtension(sourceAssetPath));
        IReadOnlyList<TextureImporterRegistration> registrations = GetTextureImporterRegistrations();
        for (int registrationIndex = 0; registrationIndex < registrations.Count; registrationIndex++) {
            TextureImporterRegistration registration = registrations[registrationIndex];
            string[] extensions = registration.Extensions;
            for (int extensionIndex = 0; extensionIndex < extensions.Length; extensionIndex++) {
                if (string.Equals(extensions[extensionIndex], normalizedExtension, StringComparison.OrdinalIgnoreCase)) {
                    return registration;
                }
            }
        }

        throw new InvalidOperationException($"No default editor texture importer supports extension '{normalizedExtension}' for source texture '{sourceAssetPath}'.");
    }

    /// <summary>
    /// Loads one source font atlas asset from either a packaged <c>.hefont</c> document, one serialized texture-asset payload, one raw source texture file, or one raw source font file.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source font path emitted by the editor build graph.</param>
    /// <returns>Imported or deserialized raw atlas texture asset.</returns>
    TextureAsset LoadSourceFontAtlasTexture(string sourceAssetPath) {
        if (string.Equals(Path.GetExtension(sourceAssetPath), ".hefont", StringComparison.OrdinalIgnoreCase)) {
            using FileStream stream = new FileStream(sourceAssetPath, FileMode.Open, FileAccess.Read, FileShare.Read);
            FontAsset fontAsset = global::helengine.files.FontAssetBinarySerializer.Deserialize(stream);
            if (fontAsset.SourceTextureAsset == null) {
                throw new InvalidOperationException($"Packaged font source '{sourceAssetPath}' did not provide an atlas texture payload.");
            }

            return fontAsset.SourceTextureAsset;
        }

        if (string.Equals(Path.GetExtension(sourceAssetPath), ".hasset", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(Path.GetExtension(sourceAssetPath), ".hetex", StringComparison.OrdinalIgnoreCase)) {
            using FileStream stream = new FileStream(sourceAssetPath, FileMode.Open, FileAccess.Read, FileShare.Read);
            return global::helengine.files.AssetSerializer.Deserialize(stream) as TextureAsset
                ?? throw new InvalidOperationException($"Serialized texture source '{sourceAssetPath}' did not contain a TextureAsset payload.");
        }

        if (CanResolveTextureImporter(sourceAssetPath)) {
            TextureImporterRegistration importer = ResolveTextureImporter(sourceAssetPath);
            using FileStream stream = new FileStream(sourceAssetPath, FileMode.Open, FileAccess.Read, FileShare.Read);
            TextureAsset importedTextureAsset = importer.Importer.ImportTexture(stream);
            if (importedTextureAsset == null) {
                throw new InvalidOperationException($"Texture importer '{importer.ImporterId}' did not return a texture asset for '{sourceAssetPath}'.");
            }

            return importedTextureAsset;
        }

        FontAsset importedFontAsset = ImportRawFontAsset(sourceAssetPath);
        if (importedFontAsset.SourceTextureAsset == null) {
            throw new InvalidOperationException($"Source font '{sourceAssetPath}' did not provide an atlas texture payload.");
        }

        return importedFontAsset.SourceTextureAsset;
    }

    /// <summary>
    /// Returns whether one source path can be imported through the default editor texture importer registrations.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path.</param>
    /// <returns>True when one registered texture importer supports the source extension; otherwise false.</returns>
    bool CanResolveTextureImporter(string sourceAssetPath) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source texture path must be provided.", nameof(sourceAssetPath));
        }

        string normalizedExtension = NormalizeExtension(Path.GetExtension(sourceAssetPath));
        IReadOnlyList<TextureImporterRegistration> registrations = GetTextureImporterRegistrations();
        for (int registrationIndex = 0; registrationIndex < registrations.Count; registrationIndex++) {
            TextureImporterRegistration registration = registrations[registrationIndex];
            string[] extensions = registration.Extensions;
            for (int extensionIndex = 0; extensionIndex < extensions.Length; extensionIndex++) {
                if (string.Equals(extensions[extensionIndex], normalizedExtension, StringComparison.OrdinalIgnoreCase)) {
                    return true;
                }
            }
        }

        return false;
    }

    /// <summary>
    /// Imports one raw source font file through the editor host font importer.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source font path emitted by the editor build graph.</param>
    /// <returns>Imported runtime font asset.</returns>
    FontAsset ImportRawFontAsset(string sourceAssetPath) {
        IFontImporter importer = GetFontImporter();
        using FileStream fontStream = new FileStream(sourceAssetPath, FileMode.Open, FileAccess.Read, FileShare.Read);
        FontAsset importedFontAsset = importer.ImportFont(fontStream, new FontAssetProcessorSettings {
            PixelSize = FontAssetProcessorSettings.DefaultPixelSize
        });
        if (importedFontAsset == null) {
            throw new InvalidOperationException($"Font importer '{importer.GetType().FullName}' did not return a font asset for '{sourceAssetPath}'.");
        }

        return importedFontAsset;
    }

    /// <summary>
    /// Gets the lazily loaded default texture importer registrations provided by the Windows editor backend.
    /// </summary>
    /// <returns>Default texture importer registrations.</returns>
    IReadOnlyList<TextureImporterRegistration> GetTextureImporterRegistrations() {
        if (TextureImporterRegistrationsValue != null) {
            return TextureImporterRegistrationsValue;
        }

        Assembly editorWindowsAssembly = LoadEditorWindowsAssembly();
        Type textureFactoryType = editorWindowsAssembly.GetType("helengine.editor.EditorHostTextureImporterFactory", true)
            ?? throw new InvalidOperationException("Windows editor backend did not expose EditorHostTextureImporterFactory.");
        MethodInfo createDefaultMethod = textureFactoryType.GetMethod("CreateDefault", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("Windows editor backend did not expose EditorHostTextureImporterFactory.CreateDefault().");
        object result = createDefaultMethod.Invoke(null, null)
            ?? throw new InvalidOperationException("Windows editor backend returned no default texture importers.");
        IReadOnlyList<IAssetImporterRegistration> registrations = result as IReadOnlyList<IAssetImporterRegistration>
            ?? throw new InvalidOperationException("Windows editor backend returned an unexpected texture importer collection.");

        List<TextureImporterRegistration> textureRegistrations = new List<TextureImporterRegistration>();
        for (int index = 0; index < registrations.Count; index++) {
            if (registrations[index] is TextureImporterRegistration textureRegistration) {
                textureRegistrations.Add(textureRegistration);
            }
        }

        if (textureRegistrations.Count == 0) {
            throw new InvalidOperationException("Windows editor backend did not expose any texture importer registrations.");
        }

        TextureImporterRegistrationsValue = textureRegistrations;
        return TextureImporterRegistrationsValue;
    }

    /// <summary>
    /// Gets the lazily loaded default source font importer provided by the editor host.
    /// </summary>
    /// <returns>Editor-host source font importer.</returns>
    IFontImporter GetFontImporter() {
        if (FontImporterValue != null) {
            return FontImporterValue;
        }

        Assembly editorAppAssembly = LoadEditorAppAssembly();
        Type importerFactoryType = editorAppAssembly.GetType("helengine.editor.app.EditorHostImporterFactory", true)
            ?? throw new InvalidOperationException("Editor host assembly did not expose helengine.editor.app.EditorHostImporterFactory.");
        MethodInfo createDefaultMethod = importerFactoryType.GetMethod("CreateDefault", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("Editor host assembly did not expose EditorHostImporterFactory.CreateDefault().");
        object result = createDefaultMethod.Invoke(null, null)
            ?? throw new InvalidOperationException("Editor host assembly returned no default importer registrations.");
        IReadOnlyList<IAssetImporterRegistration> registrations = result as IReadOnlyList<IAssetImporterRegistration>
            ?? throw new InvalidOperationException("Editor host assembly returned an unexpected importer registration collection.");

        for (int index = 0; index < registrations.Count; index++) {
            if (registrations[index] is FontImporterRegistration fontRegistration &&
                string.Equals(fontRegistration.ImporterId, "gdi-font", StringComparison.OrdinalIgnoreCase)) {
                FontImporterValue = fontRegistration.Importer;
                return FontImporterValue;
            }
        }

        throw new InvalidOperationException("Editor host assembly did not expose a gdi-font importer registration.");
    }

    /// <summary>
    /// Loads the editor host assembly that owns the current default importer registrations.
    /// </summary>
    /// <returns>Loaded editor host assembly.</returns>
    Assembly LoadEditorAppAssembly() {
        Assembly[] loadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
        for (int index = 0; index < loadedAssemblies.Length; index++) {
            Assembly assembly = loadedAssemblies[index];
            if (string.Equals(assembly.GetName().Name, "helengine.editor.app", StringComparison.OrdinalIgnoreCase)) {
                return assembly;
            }
        }

        try {
            return Assembly.Load("helengine.editor.app");
        } catch {
        }

        string assemblyPath = Path.Combine(ResolveHelengineRootPath(), NormalizeRelativePath(EditorAppAssemblyRelativePath));
        if (!File.Exists(assemblyPath)) {
            throw new FileNotFoundException(
                "Editor host assembly 'helengine.editor.app' was not found. Build helengine.editor.app before executing PSP builder-owned font cook work items.",
                assemblyPath);
        }

        return Assembly.LoadFrom(assemblyPath);
    }

    /// <summary>
    /// Loads the Windows editor backend assembly that owns the source texture and source font import implementations.
    /// </summary>
    /// <returns>Loaded Windows editor backend assembly.</returns>
    Assembly LoadEditorWindowsAssembly() {
        Assembly[] loadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
        for (int index = 0; index < loadedAssemblies.Length; index++) {
            Assembly assembly = loadedAssemblies[index];
            if (string.Equals(assembly.GetName().Name, "helengine.editor.windows", StringComparison.OrdinalIgnoreCase)) {
                return assembly;
            }
        }

        try {
            return Assembly.Load("helengine.editor.windows");
        } catch {
        }

        string assemblyPath = Path.Combine(ResolveHelengineRootPath(), NormalizeRelativePath(EditorWindowsAssemblyRelativePath));
        if (!File.Exists(assemblyPath)) {
            throw new FileNotFoundException(
                "Windows editor backend assembly 'helengine.editor.windows' was not found. Build helengine.editor.windows before executing PSP builder-owned cook work items.",
                assemblyPath);
        }

        return Assembly.LoadFrom(assemblyPath);
    }

    /// <summary>
    /// Resolves the helengine repository root used to locate editor backend assemblies when they have not been preloaded.
    /// </summary>
    /// <returns>Absolute helengine repository root path.</returns>
    static string ResolveHelengineRootPath() {
        string configuredHelengineRootPath = Environment.GetEnvironmentVariable("HELENGINE_ROOT") ?? string.Empty;
        if (IsHelengineRootPath(configuredHelengineRootPath)) {
            return Path.GetFullPath(configuredHelengineRootPath);
        }

        string configuredPspRepositoryRootPath = Environment.GetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT") ?? string.Empty;
        if (!string.IsNullOrWhiteSpace(configuredPspRepositoryRootPath)) {
            string siblingHelengineRootPath = Path.GetFullPath(Path.Combine(configuredPspRepositoryRootPath, "..", "helengine"));
            if (IsHelengineRootPath(siblingHelengineRootPath)) {
                return siblingHelengineRootPath;
            }
        }

        string currentPath = AppContext.BaseDirectory;
        while (!string.IsNullOrWhiteSpace(currentPath)) {
            string siblingHelengineRootPath = Path.GetFullPath(Path.Combine(currentPath, "..", "..", "..", "..", "..", "helengine"));
            if (IsHelengineRootPath(siblingHelengineRootPath)) {
                return siblingHelengineRootPath;
            }

            DirectoryInfo parentDirectory = Directory.GetParent(currentPath);
            if (parentDirectory == null) {
                break;
            }

            currentPath = parentDirectory.FullName;
        }

        throw new InvalidOperationException("Could not resolve the helengine repository root needed to load editor importer backends.");
    }

    /// <summary>
    /// Returns whether one path looks like the root of the sibling helengine repository.
    /// </summary>
    /// <param name="path">Candidate helengine repository root path.</param>
    /// <returns>True when the path contains the expected editor project files.</returns>
    static bool IsHelengineRootPath(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            return false;
        }

        string editorProjectPath = Path.Combine(path, "engine", "helengine.editor", "helengine.editor.csproj");
        string filesProjectPath = Path.Combine(path, "engine", "helengine.files", "helengine.files.csproj");
        return File.Exists(editorProjectPath) && File.Exists(filesProjectPath);
    }

    /// <summary>
    /// Normalizes one file extension to lowercase with a leading period.
    /// </summary>
    /// <param name="extension">Source extension value.</param>
    /// <returns>Normalized extension value.</returns>
    static string NormalizeExtension(string extension) {
        if (string.IsNullOrWhiteSpace(extension)) {
            throw new ArgumentException("Extension must be provided.", nameof(extension));
        }

        return extension.StartsWith(".", StringComparison.Ordinal)
            ? extension.ToLowerInvariant()
            : "." + extension.ToLowerInvariant();
    }

    /// <summary>
    /// Normalizes one relative filesystem path to the current directory separator convention.
    /// </summary>
    /// <param name="relativePath">Relative path to normalize.</param>
    /// <returns>Normalized filesystem path.</returns>
    static string NormalizeRelativePath(string relativePath) {
        if (string.IsNullOrWhiteSpace(relativePath)) {
            throw new ArgumentException("Relative path must be provided.", nameof(relativePath));
        }

        return relativePath.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
    }
}
