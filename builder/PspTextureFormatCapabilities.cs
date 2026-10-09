using helengine.baseplatform.Definitions;

namespace helengine.psp.builder;

/// <summary>Publishes verified native PSP settings alongside existing image and font defaults.</summary>
public static class PspTextureFormatCapabilities {
    /// <summary>Creates the shared texture/font capability matrix with an explicit alpha combination for every setting.</summary>
    public static PlatformTextureFormatCapabilityDefinition Create() {
        List<string> ids = new List<string> { "Rgba4444", "Indexed8", "Rgba32", "Indexed4" };
        List<PlatformTextureFormatCombinationDefinition> combinations = new List<PlatformTextureFormatCombinationDefinition>();
        foreach (string id in ids) {
            TextureAssetAlphaPrecision[] alphas = id == "Rgba4444" ? new[] { TextureAssetAlphaPrecision.Opaque, TextureAssetAlphaPrecision.A4 }
                : new[] { TextureAssetAlphaPrecision.Opaque, TextureAssetAlphaPrecision.Binary, TextureAssetAlphaPrecision.A4, TextureAssetAlphaPrecision.A8 };
            foreach (TextureAssetAlphaPrecision alpha in alphas) combinations.Add(new PlatformTextureFormatCombinationDefinition(id, alpha));
        }
        foreach (PspTextureFormat format in PspTextureFormatCatalog.Formats) {
            ids.Add(format.Id);
            combinations.Add(new PlatformTextureFormatCombinationDefinition(format.Id, TextureAssetAlphaPrecision.Opaque));
            if (format.Alpha != TextureAssetAlphaPrecision.Opaque) combinations.Add(new PlatformTextureFormatCombinationDefinition(format.Id, format.Alpha));
        }
        return new PlatformTextureFormatCapabilityDefinition(ids.ToArray(), new[] { TextureAssetAlphaPrecision.Opaque, TextureAssetAlphaPrecision.Binary, TextureAssetAlphaPrecision.A4, TextureAssetAlphaPrecision.A8 }, combinations.ToArray());
    }
}
