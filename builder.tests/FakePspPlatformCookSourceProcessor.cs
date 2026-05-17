using helengine.editor;

namespace helengine.psp.builder.tests;

/// <summary>
/// Provides deterministic builder-owned texture and font cook outputs for PSP builder tests.
/// </summary>
public sealed class FakePspPlatformCookSourceProcessor : IPspPlatformCookSourceProcessor {
    /// <summary>
    /// Texture asset returned by the fake source processor.
    /// </summary>
    readonly TextureAsset TextureAssetValue;

    /// <summary>
    /// Font asset returned by the fake source processor.
    /// </summary>
    readonly FontAsset FontAssetValue;

    /// <summary>
    /// Initializes one deterministic fake source processor.
    /// </summary>
    /// <param name="textureAsset">Texture asset returned for texture work items.</param>
    /// <param name="fontAsset">Font asset returned for font work items.</param>
    public FakePspPlatformCookSourceProcessor(TextureAsset textureAsset, FontAsset fontAsset) {
        TextureAssetValue = textureAsset ?? throw new ArgumentNullException(nameof(textureAsset));
        FontAssetValue = fontAsset ?? throw new ArgumentNullException(nameof(fontAsset));
    }

    /// <summary>
    /// Returns a cloned deterministic texture asset for the supplied work item inputs.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked texture should preserve.</param>
    /// <param name="settings">Resolved PSP texture processor settings supplied by the editor.</param>
    /// <returns>Deterministic processed runtime texture payload.</returns>
    public TextureAsset CookTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source texture path must be provided.", nameof(sourceAssetPath));
        } else if (string.IsNullOrWhiteSpace(assetId)) {
            throw new ArgumentException("Texture asset id must be provided.", nameof(assetId));
        } else if (settings == null) {
            throw new ArgumentNullException(nameof(settings));
        }

        return new TextureAsset {
            Id = assetId,
            RuntimeAssetId = RuntimeAssetIdGenerator.Generate(assetId),
            Width = TextureAssetValue.Width,
            Height = TextureAssetValue.Height,
            ColorFormat = TextureAssetValue.ColorFormat,
            AlphaPrecision = TextureAssetValue.AlphaPrecision,
            PaletteColors = TextureAssetValue.PaletteColors == null ? null : [.. TextureAssetValue.PaletteColors],
            Colors = TextureAssetValue.Colors == null ? null : [.. TextureAssetValue.Colors]
        };
    }

    /// <summary>
    /// Returns a cloned deterministic font asset for the supplied work item inputs.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source font path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked font should preserve for atlas ownership.</param>
    /// <param name="settings">Resolved PSP texture processor settings supplied by the editor.</param>
    /// <returns>Deterministic processed runtime font payload.</returns>
    public FontAsset CookFont(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source font path must be provided.", nameof(sourceAssetPath));
        } else if (string.IsNullOrWhiteSpace(assetId)) {
            throw new ArgumentException("Font asset id must be provided.", nameof(assetId));
        } else if (settings == null) {
            throw new ArgumentNullException(nameof(settings));
        }

        Dictionary<char, FontChar> characters = new Dictionary<char, FontChar>();
        if (FontAssetValue.Characters != null) {
            foreach (KeyValuePair<char, FontChar> entry in FontAssetValue.Characters) {
                characters[entry.Key] = entry.Value;
            }
        }

        TextureAsset sourceTextureAsset = FontAssetValue.SourceTextureAsset ?? throw new InvalidOperationException("Fake font assets must include one source texture asset.");
        FontAsset clonedFontAsset = new FontAsset(
            FontAssetValue.FontInfo == null
                ? throw new InvalidOperationException("Fake font assets must include font metrics.")
                : new FontInfo(FontAssetValue.FontInfo.Name, FontAssetValue.FontInfo.LineSpacing, FontAssetValue.FontInfo.SpaceWidth),
            new FakeRuntimeTexture {
                Width = FontAssetValue.AtlasWidth,
                Height = FontAssetValue.AtlasHeight
            },
            characters,
            FontAssetValue.LineHeight,
            FontAssetValue.AtlasWidth,
            FontAssetValue.AtlasHeight);
        clonedFontAsset.SourceTextureAsset = new TextureAsset {
            Id = assetId + "#atlas",
            RuntimeAssetId = RuntimeAssetIdGenerator.Generate(assetId + "#atlas"),
            Width = sourceTextureAsset.Width,
            Height = sourceTextureAsset.Height,
            ColorFormat = sourceTextureAsset.ColorFormat,
            AlphaPrecision = sourceTextureAsset.AlphaPrecision,
            PaletteColors = sourceTextureAsset.PaletteColors == null ? null : [.. sourceTextureAsset.PaletteColors],
            Colors = sourceTextureAsset.Colors == null ? null : [.. sourceTextureAsset.Colors]
        };
        return clonedFontAsset;
    }
}
