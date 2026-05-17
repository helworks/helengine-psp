using helengine.editor;

namespace helengine.psp.builder;

/// <summary>
/// Imports builder-owned PSP source assets and converts them into final runtime payloads.
/// </summary>
public interface IPspPlatformCookSourceProcessor {
    /// <summary>
    /// Imports one source texture asset and applies the resolved PSP texture processor settings.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked texture should preserve.</param>
    /// <param name="settings">Resolved PSP texture processor settings supplied by the editor.</param>
    /// <returns>Processed runtime texture payload ready for serialization.</returns>
    TextureAsset CookTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings);

    /// <summary>
    /// Imports one source font asset and applies the resolved PSP atlas texture processor settings.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source font path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked font should preserve for atlas ownership.</param>
    /// <param name="settings">Resolved PSP texture processor settings supplied by the editor.</param>
    /// <returns>Processed runtime font payload ready for serialization.</returns>
    FontAsset CookFont(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings);
}
