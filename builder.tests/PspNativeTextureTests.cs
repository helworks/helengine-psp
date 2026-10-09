using helengine.editor;

namespace helengine.psp.builder.tests;

/// <summary>Checks hardware byte layouts, malformed descriptors, GIM import and real builder texture processing.</summary>
public sealed class PspNativeTextureTests {
    /// <summary>Enumerates every native format, palette and storage-order setting.</summary>
    public static IEnumerable<object[]> Formats() => PspTextureFormatCatalog.Formats.Select(format => new object[] { format.Id });

    /// <summary>Verifies all settings survive serialization and emit bounded native pixels with matching VRAM accounting.</summary>
    [Theory]
    [MemberData(nameof(Formats))]
    public void Native_formats_cook_preview_and_serialize(string id) {
        PspTextureFormatCatalog.TryGetFormat(id, out PspTextureFormat format);
        TextureAssetProcessorSettings settings = new TextureAssetProcessorSettings { ColorFormatId = id, AlphaPrecision = format.Alpha, MaxResolution = 512 };
        Assert.Equal(id, PspTextureCookSettingsSerializer.Deserialize(PspTextureCookSettingsSerializer.Serialize(settings)).ColorFormatId);
        using TextureAsset source = Source(13, 9);
        using TextureAsset cooked = PspTextureCooker.Apply(source, settings);
        PspTextureLayout layout = PspTextureCodec.ReadLayout(cooked);
        byte[] preview = TextureAssetPixelCodec.DecodeToRgba32(cooked);
        Assert.Equal(13 * 9 * 4, preview.Length);
        Assert.Equal(source.Id, cooked.Id);
        Assert.Equal(source.RuntimeAssetId, cooked.RuntimeAssetId);
        Assert.Equal(TextureAssetColorFormat.PspNative, cooked.ColorFormat);
        Assert.True(TextureVramUsageCalculator.TryCalculateBytes(13, 9, settings, out long bytes));
        Assert.Equal(layout.TexelBytes + layout.PaletteBytes, bytes);
        using MemoryStream serialized = new MemoryStream(global::helengine.files.AssetSerializer.SerializeToBytes(cooked));
        using TextureAsset restored = Assert.IsType<TextureAsset>(global::helengine.files.AssetSerializer.Deserialize(serialized));
        Assert.Equal(preview, PspTextureCodec.Decode(restored));
        serialized.Position = 0;
        using TextureAsset runtime = Assert.IsType<TextureAsset>(PackagedAssetBinarySerializer.Deserialize(serialized));
        Assert.Equal(cooked.Colors, runtime.Colors);
        Assert.Equal(cooked.PaletteColors, runtime.PaletteColors);
        Assert.Equal(preview, PspTextureCodec.Decode(runtime));
        WriteFixture(format, cooked, preview);
        foreach (int offset in new[] { 0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44 }) {
            byte original = cooked.Colors[offset];
            cooked.Colors[offset] = 255;
            Assert.ThrowsAny<ArgumentException>(() => PspTextureCodec.ReadLayout(cooked));
            cooked.Colors[offset] = original;
        }
    }
    /// <summary>Checks little-endian PSP channels independently from the decoder's inverse mapping.</summary>
    [Theory]
    [InlineData("Psp.Linear.Rgb565", 0x1f, 0x00)]
    [InlineData("Psp.Linear.Rgba5551", 0x1f, 0x80)]
    [InlineData("Psp.Linear.Rgba4444", 0x0f, 0xf0)]
    public void Direct_red_has_native_low_channel_bits(string id, byte low, byte high) {
        PspTextureFormatCatalog.TryGetFormat(id, out PspTextureFormat format);
        using TextureAsset red = new TextureAsset { Width = 1, Height = 1, Colors = new byte[] { 255, 0, 0, 255 }, ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8 };
        using TextureAsset cooked = PspTextureCodec.Encode(red, id, format.Alpha);
        Assert.Equal(low, cooked.Colors[48]); Assert.Equal(high, cooked.Colors[49]);
    }
    /// <summary>Checks the fixed PSP byte-block swizzle using literal expected addresses across both block axes.</summary>
    [Fact]
    public void Swizzle_addresses_are_16_byte_by_8_row_blocks() {
        PspTextureFormatCatalog.TryGetFormat("Psp.Swizzled.Rgba8888", out PspTextureFormat format);
        PspTextureLayout layout = new PspTextureLayout(format, 16, 16);
        Assert.Equal(0, layout.ByteOffset(0, 0)); Assert.Equal(16, layout.ByteOffset(0, 1));
        Assert.Equal(128, layout.ByteOffset(16, 0)); Assert.Equal(512, layout.ByteOffset(0, 8));
    }
    /// <summary>Checks PSP DXT5 color/alpha ordering against an independently specified standard BC3 block.</summary>
    [Fact]
    public void Dxt5_uses_selectors_endpoints_and_reversed_alpha_groups() {
        byte[] standard = Enumerable.Range(0, 16).Select(value => (byte)value).ToArray();
        byte[] native = new byte[16];
        PspTextureCodec.ConvertDxt(standard, 0, native, 0, 10, 16, true);
        Assert.Equal(new byte[] { 12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 2, 3, 0, 1 }, native);
    }
    /// <summary>Decodes an external PSP DXT1 red block whose endpoints follow its selectors.</summary>
    [Fact]
    public void External_psp_dxt1_block_decodes_red() {
        byte[] block = { 0, 0, 0, 0, 0, 0xf8, 0, 0 };
        byte[] rgba = PspTextureCodec.DecodePixels(block, 0, null, 8, 0, 4, 4, 8, 4, false);
        for (int pixel = 0; pixel < 16; pixel++) Assert.Equal(new byte[] { 255, 0, 0, 255 }, rgba.Skip(pixel * 4).Take(4));
    }
    /// <summary>Rejects upper index bits in canonical T16 storage instead of silently aliasing a malformed palette index.</summary>
    [Fact]
    public void Canonical_t16_rejects_indices_above_255() {
        using TextureAsset source = Source(2, 2);
        using TextureAsset cooked = PspTextureCooker.Apply(source, new TextureAssetProcessorSettings { ColorFormatId = "Psp.Linear.T16.ClutRgba8888", AlphaPrecision = TextureAssetAlphaPrecision.A8 });
        cooked.Colors[49] = 1;
        Assert.Throws<ArgumentException>(() => PspTextureCodec.ReadLayout(cooked));
    }
    /// <summary>Verifies every hardware encoding imports through a real GIM container with matching base pixels.</summary>
    [Theory]
    [MemberData(nameof(Formats))]
    public void Gim_imports_native_formats_and_palette_encodings(string id) {
        PspTextureFormatCatalog.TryGetFormat(id, out PspTextureFormat format);
        using TextureAsset source = Source(32, 8);
        using TextureAsset cooked = PspTextureCooker.Apply(source, new TextureAssetProcessorSettings { ColorFormatId = id, AlphaPrecision = format.Alpha });
        byte[] container = PspGimTestFile.Create(cooked);
        using TextureAsset imported = new PspGimTextureImporter().ImportTexture(new MemoryStream(container));
        Assert.Equal(PspTextureCodec.Decode(cooked), imported.Colors);
    }
    /// <summary>Rejects offsets that point into image metadata or past the owning GIM block.</summary>
    [Fact]
    public void Gim_rejects_invalid_block_and_pixel_table_offsets() {
        using TextureAsset source = Source(32, 8);
        using TextureAsset cooked = PspTextureCodec.Encode(source, "Psp.Linear.Rgba8888", TextureAssetAlphaPrecision.A8);
        byte[] container = PspGimTestFile.Create(cooked);
        foreach (int offset in new[] { 20, 24, 36, 40, 52, 60, 88, 92, 96, 112 }) {
            byte original = container[offset]; container[offset] = 255;
            Assert.ThrowsAny<Exception>(() => new PspGimTextureImporter().ImportTexture(new MemoryStream(container)));
            container[offset] = original;
        }
    }
    /// <summary>Runs the actual source processor on GIM without requiring Windows backend loading.</summary>
    [Fact]
    public void Source_processor_cooks_gim_and_resizes_before_native_encoding() {
        string root = OutputRoot();
        using TextureAsset source = Source(32, 8);
        using TextureAsset cooked = PspTextureCodec.Encode(source, "Psp.Linear.Rgba8888", TextureAssetAlphaPrecision.A8);
        string path = Path.Combine(root, "source.gim"); File.WriteAllBytes(path, PspGimTestFile.Create(cooked));
        using TextureAsset output = new PspPlatformCookSourceProcessor().CookTexture(path, "texture:psp-test", new TextureAssetProcessorSettings {
            ColorFormatId = "Psp.Swizzled.Rgba4444", AlphaPrecision = TextureAssetAlphaPrecision.A4, MaxResolution = 16 });
        Assert.Equal(16, output.Width); Assert.Equal(4, output.Height);
        Assert.Equal("texture:psp-test", output.Id); PspTextureCodec.ReadLayout(output);
    }
    /// <summary>Checks hardware limits and incompatible alpha settings at the public cooker boundary.</summary>
    [Fact]
    public void Invalid_settings_and_oversize_images_fail_explicitly() {
        Assert.Throws<ArgumentException>(() => PspTextureCooker.ValidateSettings(new TextureAssetProcessorSettings { ColorFormatId = "Psp.Linear.Rgb565", AlphaPrecision = TextureAssetAlphaPrecision.A8 }));
        Assert.Throws<ArgumentException>(() => PspTextureCooker.ValidateSettings(new TextureAssetProcessorSettings { ColorFormatId = "Psp.Swizzled.Dxt1" }));
        Assert.Throws<ArgumentException>(() => PspTextureCooker.ValidateSettings(new TextureAssetProcessorSettings { MaxResolution = 513 }));
        Assert.Throws<ArgumentOutOfRangeException>(() => new PspTextureLayout(PspTextureFormatCatalog.Formats[0], 513, 1));
    }
    /// <summary>Creates a small deterministic colored alpha image with edge padding exercised by native storage.</summary>
    static TextureAsset Source(int width, int height) {
        byte[] pixels = new byte[width * height * 4];
        for (int index = 0; index < width * height; index++) {
            pixels[index * 4] = (byte)(index * 37); pixels[index * 4 + 1] = (byte)(index * 17); pixels[index * 4 + 2] = (byte)(index * 11); pixels[index * 4 + 3] = (byte)(index * 23);
        }
        return new TextureAsset { Id = "texture:fixture", RuntimeAssetId = 123, Width = (ushort)width, Height = (ushort)height, ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8, Colors = pixels };
    }
    /// <summary>Resolves a visible fixture output directory supplied by the validation script.</summary>
    static string OutputRoot() {
        string root = Environment.GetEnvironmentVariable("HELENGINE_PSP_TEXTURE_TEST_OUTPUT") ?? Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "psp-texture-fixtures"));
        Directory.CreateDirectory(root); return root;
    }
    /// <summary>Writes actual managed native payloads and previews for the independent C++ parser/decoder check.</summary>
    static void WriteFixture(PspTextureFormat format, TextureAsset asset, byte[] preview) {
        string prefix = Path.Combine(OutputRoot(), format.Id);
        File.WriteAllBytes(prefix + ".pgt", asset.Colors);
        File.WriteAllBytes(prefix + ".pal", asset.PaletteColors ?? Array.Empty<byte>());
        File.WriteAllBytes(prefix + ".rgba", preview);
    }
}
