namespace helengine.psp.builder.tests;

/// <summary>
/// Provides a deterministic source texture importer for editor-owned PSP cook verification.
/// </summary>
public sealed class PspBuilderTestTextureImporter : helengine.editor.ITextureImporter {
    /// <summary>
    /// Imports one deterministic texture asset from the supplied source stream.
    /// </summary>
    /// <param name="stream">Source texture stream used only to satisfy the importer contract.</param>
    /// <returns>Deterministic texture asset for editor-owned PSP cook tests.</returns>
    public TextureAsset ImportTexture(Stream stream) {
        if (stream == null) {
            throw new ArgumentNullException(nameof(stream));
        }

        return new TextureAsset {
            Width = 2,
            Height = 2,
            Colors = [
                255, 0, 0, 255,
                0, 255, 0, 255,
                0, 0, 255, 255,
                255, 255, 255, 255
            ]
        };
    }
}
