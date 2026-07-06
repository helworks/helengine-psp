using System;

namespace helengine.psp.builder.tests {
    /// <summary>
    /// Guards the shared scene packagers against mutating authored material settings during PSP packaging.
    /// </summary>
    public sealed class PspMaterialPackagingSourceTests {
        /// <summary>
        /// Ensures the editor build scene packager loads persisted material settings for cooking without calling the mutating load-or-create path.
        /// </summary>
        [Fact]
        public void EditorWindowsBuildScenePackager_material_cook_settings_load_is_non_mutating() {
            const string sourcePath = @"C:\dev\helworks\helengine\engine\helengine.editor\managers\project\EditorWindowsBuildScenePackager.cs";
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("MaterialAssetSettingsService.LoadOrCreateInMemory(", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("MaterialAssetSettingsService.LoadOrCreate(", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures the scene-component packaging transform service also avoids mutating authored material settings during PSP packaging.
        /// </summary>
        [Fact]
        public void SceneComponentPackagingTransformService_material_cook_settings_load_is_non_mutating() {
            const string sourcePath = @"C:\dev\helworks\helengine\engine\helengine.editor\managers\project\SceneComponentPackagingTransformService.cs";
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("MaterialAssetSettingsService.LoadOrCreateInMemory(", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("MaterialAssetSettingsService.LoadOrCreate(", sourceContents, StringComparison.Ordinal);
        }
    }
}
