namespace helengine.psp.builder.tests;

/// <summary>Builds standard GIM block/index containers around checked native bytes for importer interoperability tests.</summary>
internal static class PspGimTestFile {
    /// <summary>Creates one little-endian PSP picture containing an image and its optional direct-color palette.</summary>
    public static byte[] Create(TextureAsset texture) {
        PspTextureLayout layout = PspTextureCodec.ReadLayout(texture);
        byte[] image = Block(layout.Format.Code, layout.Format.Swizzled, layout.Width, layout.Height, texture.Colors.Skip(48).ToArray(), 4);
        byte[] palette = layout.PaletteEntries == 0 ? Array.Empty<byte>() : Block(layout.Format.PaletteCode, false, layout.PaletteEntries, 1, texture.PaletteColors, 5);
        byte[] result = new byte[48 + image.Length + palette.Length];
        new byte[] { 77, 73, 71, 46, 48, 48, 46, 49, 80, 83, 80, 0 }.CopyTo(result, 0);
        Header(result, 16, 2, result.Length - 16, 16); Header(result, 32, 3, result.Length - 32, 16);
        image.CopyTo(result, 48); palette.CopyTo(result, 48 + image.Length);
        return result;
    }
    /// <summary>Writes a standard root, picture or leaf header with offsets relative to the block header.</summary>
    static void Header(byte[] output, int offset, int type, int size, int next) {
        PspTextureCodec.Write(output, offset, (uint)type, 2); PspTextureCodec.Write(output, offset + 4, (uint)size, 4);
        PspTextureCodec.Write(output, offset + 8, (uint)next, 4); PspTextureCodec.Write(output, offset + 12, 16, 4);
    }
    /// <summary>Writes one image-data descriptor, frame index and native pixel plane.</summary>
    static byte[] Block(int code, bool swizzled, int width, int height, byte[] pixels, int type) {
        byte[] block = new byte[80 + pixels.Length]; Header(block, 0, type, block.Length, block.Length);
        PspTextureCodec.Write(block, 16, 48, 2); PspTextureCodec.Write(block, 20, (uint)code, 2); PspTextureCodec.Write(block, 22, swizzled ? 1u : 0u, 2);
        PspTextureCodec.Write(block, 24, (uint)width, 2); PspTextureCodec.Write(block, 26, (uint)height, 2);
        PspTextureCodec.Write(block, 28, (uint)new PspTextureFormat(string.Empty, code, false, 0).BitsPerPixel, 2);
        PspTextureCodec.Write(block, 30, 16, 2); PspTextureCodec.Write(block, 32, swizzled ? 8u : 1u, 2);
        PspTextureCodec.Write(block, 40, 48, 4); PspTextureCodec.Write(block, 44, 64, 4); PspTextureCodec.Write(block, 48, (uint)(64 + pixels.Length), 4);
        PspTextureCodec.Write(block, 56, type == 5 ? 2u : 1u, 2); PspTextureCodec.Write(block, 58, 1, 2);
        PspTextureCodec.Write(block, 60, 3, 2); PspTextureCodec.Write(block, 62, 1, 2); PspTextureCodec.Write(block, 64, 64, 4);
        pixels.CopyTo(block, 80); return block;
    }
}
