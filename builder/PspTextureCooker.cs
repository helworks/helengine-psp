using helengine.editor;

namespace helengine.psp.builder;

/// <summary>Applies resizing, palette quantization and native PSP storage without expanding the runtime GPU texture.</summary>
public static class PspTextureCooker {
    /// <summary>Processes an imported image or font atlas using the exact platform-published format and alpha policy.</summary>
    public static TextureAsset Apply(TextureAsset source, TextureAssetProcessorSettings settings) {
        ValidateSettings(settings);
        TextureAssetProcessor processor = new TextureAssetProcessor();
        bool native = PspTextureFormatCatalog.TryGetFormat(settings.ColorFormatId, out PspTextureFormat format);
        TextureAssetProcessorSettings generic = new TextureAssetProcessorSettings {
            MaxResolution = settings.MaxResolution,
            ColorFormatId = native ? (format.IsIndexed ? format.Code == 4 ? "Indexed4" : "Indexed8" : "Rgba32") : settings.ColorFormatId,
            AlphaPrecision = settings.AlphaPrecision,
            IndexingMethodId = settings.IndexingMethodId
        };
        TextureAsset processed = processor.Apply(source, generic);
        if (processed.Width > 512 || processed.Height > 512) {
            if (!ReferenceEquals(processed, source)) processed.Dispose();
            throw new InvalidOperationException("Cooked PSP textures must fit within the GU limit of 512 by 512.");
        }
        if (!native) return processed;
        try { return PspTextureCodec.Encode(processed, settings.ColorFormatId, settings.AlphaPrecision); }
        finally { if (!ReferenceEquals(processed, source)) processed.Dispose(); }
    }
    /// <summary>Rejects unpublished formats, incompatible alpha and resolution limits before importing source data.</summary>
    public static void ValidateSettings(TextureAssetProcessorSettings settings) {
        if (settings == null) throw new ArgumentNullException(nameof(settings));
        if (settings.MaxResolution < 0 || settings.MaxResolution > 512) throw new ArgumentException("PSP maximum resolution must be zero or within 1..512.");
        if (PspTextureFormatCatalog.TryGetFormat(settings.ColorFormatId, out PspTextureFormat native)) {
            if (!native.SupportsAlpha(settings.AlphaPrecision)) throw new ArgumentException("The PSP native format does not support the requested alpha precision.");
        } else if (settings.ColorFormatId == "Rgba32" || settings.ColorFormatId == "Indexed4" || settings.ColorFormatId == "Indexed8") {
            if (settings.AlphaPrecision != TextureAssetAlphaPrecision.Opaque && settings.AlphaPrecision != TextureAssetAlphaPrecision.Binary && settings.AlphaPrecision != TextureAssetAlphaPrecision.A4 && settings.AlphaPrecision != TextureAssetAlphaPrecision.A8) throw new ArgumentException("Unsupported PSP alpha precision.");
        } else if (settings.ColorFormatId == "Rgba4444") {
            if (settings.AlphaPrecision != TextureAssetAlphaPrecision.Opaque && settings.AlphaPrecision != TextureAssetAlphaPrecision.A4) throw new ArgumentException("PSP RGBA4444 requires opaque or four-bit alpha.");
        } else throw new ArgumentException("Unknown PSP texture format identifier.");
        if (settings.UsesIndexedColorFormat()) settings.ResolveIndexingMethod();
        else if (!string.IsNullOrEmpty(settings.IndexingMethodId)) throw new ArgumentException("PSP indexing settings require an indexed format.");
    }
}
