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
    /// Lazily initialized default texture importer registrations provided by the portable editor host.
    /// </summary>
    IReadOnlyList<TextureImporterRegistration> TextureImporterRegistrationsValue;

    /// <summary>
    /// Lazily initialized source font importer provided by the portable editor host.
    /// </summary>
    IFontImporter FontImporterValue;

    /// <summary>
    /// Initializes the shared source processor used by PSP builder-owned work items.
    /// </summary>
    public PspPlatformCookSourceProcessor() {
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

        TextureAsset cookedTextureAsset;
        try { cookedTextureAsset = PspTextureCooker.Apply(importedTextureAsset, settings); }
        catch { importedTextureAsset.Dispose(); throw; }
        if (!ReferenceEquals(cookedTextureAsset, importedTextureAsset)) importedTextureAsset.Dispose();
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
        TextureAsset cookedAtlasAsset;
        try { cookedAtlasAsset = PspTextureCooker.Apply(importedAtlasTextureAsset, settings); }
        catch { importedAtlasTextureAsset.Dispose(); throw; }
        if (!ReferenceEquals(cookedAtlasAsset, importedAtlasTextureAsset)) importedAtlasTextureAsset.Dispose();
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
        if (normalizedExtension == ".gim") return new TextureImporterRegistration("psp-gim", new PspGimTextureImporter(), new[] { ".gim" });
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
        if (normalizedExtension == ".gim") return true;
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
    /// Gets the default portable texture importers while preserving authored importer identifiers.
    /// </summary>
    /// <returns>Default texture importer registrations.</returns>
    IReadOnlyList<TextureImporterRegistration> GetTextureImporterRegistrations() {
        if (TextureImporterRegistrationsValue != null) {
            return TextureImporterRegistrationsValue;
        }

        IReadOnlyList<IAssetImporterRegistration> registrations = PortableImporterFactory.CreateDefault();

        List<TextureImporterRegistration> textureRegistrations = new List<TextureImporterRegistration>();
        for (int index = 0; index < registrations.Count; index++) {
            if (registrations[index] is TextureImporterRegistration textureRegistration) {
                textureRegistrations.Add(textureRegistration);
            }
        }

        if (textureRegistrations.Count == 0) {
            throw new InvalidOperationException("Portable editor host did not expose any texture importer registrations.");
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

        IReadOnlyList<IAssetImporterRegistration> registrations = PortableImporterFactory.CreateDefault();

        for (int index = 0; index < registrations.Count; index++) {
            if (registrations[index] is FontImporterRegistration fontRegistration &&
                string.Equals(fontRegistration.ImporterId, "gdi-font", StringComparison.OrdinalIgnoreCase)) {
                FontImporterValue = fontRegistration.Importer;
                return FontImporterValue;
            }
        }

        throw new InvalidOperationException("Portable editor host did not expose a gdi-font importer registration.");
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

}
