using System.Text.Json;
using helengine.editor;

namespace helengine.psp.builder;

/// <summary>
/// Serializes and deserializes the generic PSP texture cook settings contract emitted by the editor build graph.
/// </summary>
public static class PspTextureCookSettingsSerializer {
    /// <summary>
    /// Serializes one PSP texture settings payload to the generic editor JSON contract.
    /// </summary>
    /// <param name="settings">Texture settings to serialize.</param>
    /// <returns>Serialized JSON payload consumed by builder-owned cook work items.</returns>
    public static string Serialize(TextureAssetProcessorSettings settings) {
        if (settings == null) {
            throw new ArgumentNullException(nameof(settings));
        }

        return JsonSerializer.Serialize(new Dictionary<string, object> {
            ["maxResolution"] = settings.MaxResolution,
            ["colorFormat"] = settings.ColorFormat.ToString(),
            ["alphaPrecision"] = settings.AlphaPrecision.ToString()
        });
    }

    /// <summary>
    /// Deserializes one PSP texture settings payload from the generic editor JSON contract.
    /// </summary>
    /// <param name="serializedSettings">Serialized JSON payload emitted by the editor build graph.</param>
    /// <returns>Resolved PSP texture processor settings.</returns>
    public static TextureAssetProcessorSettings Deserialize(string serializedSettings) {
        if (string.IsNullOrWhiteSpace(serializedSettings)) {
            throw new ArgumentException("Serialized PSP texture settings must be provided.", nameof(serializedSettings));
        }

        using JsonDocument document = JsonDocument.Parse(serializedSettings);
        JsonElement root = document.RootElement;
        int maxResolution = root.TryGetProperty("maxResolution", out JsonElement maxResolutionElement)
            ? maxResolutionElement.GetInt32()
            : 0;
        string colorFormatName = root.TryGetProperty("colorFormat", out JsonElement colorFormatElement)
            ? colorFormatElement.GetString() ?? TextureAssetColorFormat.Rgba32.ToString()
            : TextureAssetColorFormat.Rgba32.ToString();
        string alphaPrecisionName = root.TryGetProperty("alphaPrecision", out JsonElement alphaPrecisionElement)
            ? alphaPrecisionElement.GetString() ?? TextureAssetAlphaPrecision.A8.ToString()
            : TextureAssetAlphaPrecision.A8.ToString();

        if (!Enum.TryParse(colorFormatName, true, out TextureAssetColorFormat colorFormat)) {
            throw new InvalidOperationException($"Unsupported PSP texture color format '{colorFormatName}'.");
        }

        if (!Enum.TryParse(alphaPrecisionName, true, out TextureAssetAlphaPrecision alphaPrecision)) {
            throw new InvalidOperationException($"Unsupported PSP texture alpha precision '{alphaPrecisionName}'.");
        }

        return new TextureAssetProcessorSettings {
            MaxResolution = maxResolution,
            ColorFormat = colorFormat,
            AlphaPrecision = alphaPrecision
        };
    }
}
