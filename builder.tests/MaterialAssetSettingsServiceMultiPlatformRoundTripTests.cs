using helengine.editor;

namespace helengine.psp.builder.tests {
    /// <summary>
    /// Verifies the shared material-settings service preserves generated multi-platform texture bindings across a save/load round-trip.
    /// </summary>
    public sealed class MaterialAssetSettingsServiceMultiPlatformRoundTripTests {
        /// <summary>
        /// Ensures one saved standard-shader PSP platform payload retains its authored texture-id even when the same material also includes a different DS schema payload.
        /// </summary>
        [Fact]
        public void Save_and_load_round_trip_preserves_psp_texture_id_when_material_has_multiple_platform_schemas() {
            string tempRootPath = Path.Combine(Path.GetTempPath(), "helengine-psp-material-settings-roundtrip", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(tempRootPath);

            try {
                string materialAssetPath = Path.Combine(tempRootPath, "Cube00.hasset");
                MaterialAssetImportSettings settings = new MaterialAssetImportSettings();
                settings.Importer.ImporterId = "helengine.material";
                settings.Importer.SourceChecksum = string.Empty;
                settings.Importer.AssetId = "Materials.rendering.textured_cube_grid.Cube00";

                settings.Processor.Platforms["psp"] = new MaterialAssetProcessorSettings {
                    SchemaId = "standard-shader",
                    FieldValues = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) {
                        ["use-custom-shader"] = "false",
                        ["shader-asset-id"] = "ForwardStandardShader",
                        ["texture-id"] = "Textures/GeneratedChecker",
                        ["casts-shadow"] = "true",
                        ["receives-shadow"] = "true",
                        ["base-color"] = "#FFFFFFFF"
                    }
                };
                settings.Processor.Platforms["ds"] = new MaterialAssetProcessorSettings {
                    SchemaId = "ds-standard-textured",
                    FieldValues = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) {
                        ["texture-id"] = "Textures/GeneratedChecker",
                        ["texture-relative-path"] = "cooked/imported/Textures/GeneratedChecker",
                        ["double-sided"] = "false",
                        ["vertex-color-mode"] = "ignore",
                        ["base-color"] = "#FFFFFFFF",
                        ["lighting-mode"] = "lit"
                    }
                };

                MaterialAssetSettingsService service = new MaterialAssetSettingsService();
                service.Save(materialAssetPath, settings);

                Assert.True(service.TryLoadPlatformSettings(materialAssetPath, "psp", out MaterialAssetProcessorSettings platformSettings));
                Assert.NotNull(platformSettings);
                Assert.Equal("standard-shader", platformSettings.SchemaId);
                Assert.Equal("Textures/GeneratedChecker", platformSettings.FieldValues["texture-id"]);
            } finally {
                if (Directory.Exists(tempRootPath)) {
                    Directory.Delete(tempRootPath, true);
                }
            }
        }
    }
}
