using helengine.editor;

namespace helengine.psp.builder.tests {
    /// <summary>
    /// Verifies the generated city textured-cube material settings resolve the expected PSP texture binding.
    /// </summary>
    public sealed class CityTexturedCubeGridMaterialSettingsTests {
        /// <summary>
        /// Ensures the generated textured-cube material preserves the PSP standard-shader texture-id field through the shared material-settings load path.
        /// </summary>
        [Fact]
        public void City_textured_cube_grid_generated_material_settings_preserve_psp_texture_id() {
            const string materialAssetPath = @"C:\dev\helprojs\city\assets\Materials\rendering\textured_cube_grid\Cube00.hasset";
            MaterialAssetSettingsService service = new MaterialAssetSettingsService();

            Assert.True(service.TryLoadPlatformSettings(materialAssetPath, "psp", out MaterialAssetProcessorSettings platformSettings));
            Assert.NotNull(platformSettings);
            Assert.Equal("standard-shader", platformSettings.SchemaId);
            Assert.True(platformSettings.FieldValues.TryGetValue("texture-id", out string diffuseTextureAssetId));
            Assert.False(string.IsNullOrWhiteSpace(diffuseTextureAssetId));

            ShaderMaterialAsset materialAsset = service.LoadMaterialAsset(materialAssetPath, "psp");
            Assert.Equal(diffuseTextureAssetId, materialAsset.DiffuseTextureAssetId);
        }
    }
}
