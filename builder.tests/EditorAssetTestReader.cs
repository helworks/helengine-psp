namespace helengine.psp.builder.tests;

/// <summary>
/// Reads editor-authored binary assets from raw byte arrays inside PSP builder tests.
/// </summary>
public static class EditorAssetTestReader {
    /// <summary>
    /// Deserializes one editor-authored asset payload from raw bytes and verifies the requested asset type.
    /// </summary>
    /// <typeparam name="TAsset">Expected concrete asset type.</typeparam>
    /// <param name="data">Serialized editor-asset bytes.</param>
    /// <returns>Deserialized asset instance cast to the requested type.</returns>
    public static TAsset ReadAsset<TAsset>(byte[] data)
        where TAsset : Asset {
        if (data == null) {
            throw new ArgumentNullException(nameof(data));
        }

        using MemoryStream stream = new MemoryStream(data, false);
        return Assert.IsType<TAsset>(global::helengine.files.AssetSerializer.Deserialize(stream));
    }
}
