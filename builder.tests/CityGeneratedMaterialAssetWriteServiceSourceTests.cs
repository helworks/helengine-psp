using System;

namespace helengine.psp.builder.tests {
    /// <summary>
    /// Guards the city generated-material writer against stale raw-asset serialization that overwrites the shared settings document contract.
    /// </summary>
    public sealed class CityGeneratedMaterialAssetWriteServiceSourceTests {
        /// <summary>
        /// Ensures the generated material writer persists only the shared material-settings document path and does not serialize a raw material asset into the same `.hasset` file first.
        /// </summary>
        [Fact]
        public void Source_GeneratedMaterialWriter_does_not_serialize_raw_material_asset_before_saving_settings() {
            const string sourcePath = @"C:\dev\helprojs\city\assets\codebase\rendering.tools\GeneratedMaterialAssetWriteService.cs";
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("BuildImportSettings(definition)", sourceContents, StringComparison.Ordinal);
            Assert.Contains("SettingsService.Save(fullMaterialPath, settings);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("settings.Processor.Platforms[entry.Key] = BuildPlatformSettings(entry.Key, entry.Value);", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("AssetSerializer.Serialize(stream, definition.MaterialAsset);", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("SettingsService.LoadOrCreate(", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("EditorProjectBootstrapper.Create", sourceContents, StringComparison.Ordinal);
        }
    }
}
