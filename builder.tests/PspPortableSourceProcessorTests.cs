using helengine.editor;
using ImageMagick;

namespace helengine.psp.builder.tests;

/// <summary>Exercises real source imports without loading the Windows editor host.</summary>
public sealed class PspPortableSourceProcessorTests {
    /// <summary>Imports a PNG through the persisted gdi registration and encodes its PSP payload.</summary>
    [Fact]
    public void CookTexture_imports_png_without_windows_editor() {
        string directory = Path.Combine(Path.GetTempPath(), "portable-import");
        Directory.CreateDirectory(directory);
        string source = Path.Combine(directory, "source.png");
        using (MagickImage image = new MagickImage(MagickColors.Red, 1, 1)) {
            image.Write(source, MagickFormat.Png);
        }
        try {
            using TextureAsset cooked = new PspPlatformCookSourceProcessor().CookTexture(source, "texture:portable", Settings());
            Assert.Equal((ushort)1, cooked.Width);
            Assert.Equal((ushort)1, cooked.Height);
            Assert.Equal("texture:portable", cooked.Id);
            Assert.Equal(TextureAssetColorFormat.PspNative, cooked.ColorFormat);
            Assert.Equal(new byte[] { 255, 0, 0, 255 }, PspTextureCodec.Decode(cooked));
        } finally {
            File.Delete(source);
            Directory.Delete(directory);
        }
    }

    /// <summary>Rasterizes the explicit source face through gdi-font and encodes a nonempty native atlas.</summary>
    [Fact]
    public void CookFontAtlasTexture_imports_source_font_without_windows_editor() {
        string source = Path.Combine(AppContext.BaseDirectory, "fixtures", "Synthetic.ttf");
        using TextureAsset cooked = new PspPlatformCookSourceProcessor().CookFontAtlasTexture(source, "font:portable", Settings());
        Assert.Equal("font:portable#atlas", cooked.Id);
        Assert.Equal(TextureAssetColorFormat.PspNative, cooked.ColorFormat);
        Assert.True(cooked.Width > 0 && cooked.Height > 0);
        Assert.Contains(PspTextureCodec.Decode(cooked), channel => channel != 0);
    }

    /// <summary>Selects an explicit lossless PSP format for source-import verification.</summary>
    static TextureAssetProcessorSettings Settings() => new TextureAssetProcessorSettings {
        ColorFormatId = "Psp.Linear.Rgba8888",
        AlphaPrecision = TextureAssetAlphaPrecision.A8,
        MaxResolution = 512
    };
}
