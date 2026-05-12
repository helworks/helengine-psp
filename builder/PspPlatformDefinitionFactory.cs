using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;

namespace helengine.psp.builder;

/// <summary>
/// Creates the typed PSP builder metadata consumed by the editor.
/// </summary>
public static class PspPlatformDefinitionFactory {
    /// <summary>
    /// Creates the PSP platform definition for the first city cube-test milestone.
    /// </summary>
    /// <returns>The PSP platform definition.</returns>
    public static PlatformDefinition Create() {
        return new PlatformDefinition(
            "psp",
            "PlayStation Portable",
            [
                new PlatformBuildProfileDefinition(
                    "debug",
                    "Debug",
                    "Debug PSP homebrew build",
                    "psp-forward",
                    "default",
                    [
                        new PlatformSettingDefinition(
                            "texture-scale-percent",
                            "Texture Scale Percent",
                            PlatformSettingKind.Text,
                            "100",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "shader-variant-pruning",
                            "Shader Variant Pruning",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            [])
                    ])
            ],
            [
                new PlatformGraphicsProfileDefinition(
                    "psp-forward",
                    "PSP Forward",
                    "Minimal PSP forward renderer for the authored cube-test scene.",
                    [
                        new PlatformSettingDefinition(
                            "default-width",
                            "Default Width",
                            PlatformSettingKind.Text,
                            "480",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "default-height",
                            "Default Height",
                            PlatformSettingKind.Text,
                            "272",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "vsync-enabled",
                            "VSync Enabled",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "fullscreen-enabled",
                            "Fullscreen Enabled",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            [])
                    ])
            ],
            [
                new PlatformAssetRequirementDefinition(
                    "scene",
                    "Scene",
                    true,
                    ["helen"]),
                new PlatformAssetRequirementDefinition(
                    "texture",
                    "Texture",
                    false,
                    ["png", "tga", "jpg"])
            ],
            [
                new PlatformMaterialSchemaDefinition(
                    "standard-shader",
                    "Standard Shader",
                    ["psp-forward"],
                    [
                        new PlatformMaterialFieldDefinition(
                            "use-custom-shader",
                            "Use Custom Shader",
                            PlatformMaterialFieldKind.Boolean,
                            "false",
                            true,
                            []),
                        new PlatformMaterialFieldDefinition(
                            "shader-asset-id",
                            "Shader Asset",
                            PlatformMaterialFieldKind.AssetReference,
                            string.Empty,
                            true,
                            []),
                        new PlatformMaterialFieldDefinition(
                            "vertex-program",
                            "Vertex Program",
                            PlatformMaterialFieldKind.Text,
                            string.Empty,
                            true,
                            []),
                        new PlatformMaterialFieldDefinition(
                            "pixel-program",
                            "Pixel Program",
                            PlatformMaterialFieldKind.Text,
                            string.Empty,
                            true,
                            []),
                        new PlatformMaterialFieldDefinition(
                            "base-color",
                            "Base Color",
                            PlatformMaterialFieldKind.Color,
                            "#ffffff",
                            false,
                            []),
                        new PlatformMaterialFieldDefinition(
                            "lighting-response",
                            "Lighting Response",
                            PlatformMaterialFieldKind.Text,
                            "lit-directional",
                            false,
                            []),
                        new PlatformMaterialFieldDefinition(
                            "receives-lighting",
                            "Receives Lighting",
                            PlatformMaterialFieldKind.Boolean,
                            "true",
                            false,
                            []),
                        new PlatformMaterialFieldDefinition(
                            "texture-id",
                            "Diffuse Texture",
                            PlatformMaterialFieldKind.AssetReference,
                            string.Empty,
                            false,
                            []),
                        new PlatformMaterialFieldDefinition(
                            "casts-shadow",
                            "Casts Shadows",
                            PlatformMaterialFieldKind.Boolean,
                            "true",
                            false,
                            []),
                        new PlatformMaterialFieldDefinition(
                            "receives-shadow",
                            "Receives Shadows",
                            PlatformMaterialFieldKind.Boolean,
                            "true",
                            false,
                            [])
                    ])
            ],
            [
                new PlatformComponentSupportRule(
                    "helengine.meshcomponent",
                    PlatformComponentSupportKind.Transform,
                    "Mesh components are normalized during packaging.",
                    string.Empty),
                new PlatformComponentSupportRule(
                    "helengine.cameracomponent",
                    PlatformComponentSupportKind.Transform,
                    "Camera components are normalized during packaging.",
                    string.Empty),
                new PlatformComponentSupportRule(
                    "helengine.directionallightcomponent",
                    PlatformComponentSupportKind.PassThrough,
                    "Directional light payloads can deserialize even when the first PSP renderer ignores them.",
                    string.Empty),
                new PlatformComponentSupportRule(
                    "helengine.directionalshadowtowerspincomponent",
                    PlatformComponentSupportKind.PassThrough,
                    "Directional cube-grid spin components can deserialize so PSP can exercise authored scene motion.",
                    string.Empty),
                new PlatformComponentSupportRule(
                    "city.menu.demodiscreturntomenucomponent, gameplay",
                    PlatformComponentSupportKind.PassThrough,
                    "Menu return gameplay components can deserialize without blocking PSP rendering-scene bring-up.",
                    string.Empty)
            ],
            [
                new PlatformCodegenProfileDefinition(
                    "default",
                    "Default",
                    "PSP C# to C++ codegen profile",
                    PlatformCodegenLanguage.Cpp,
                    PlatformSerializationEndianness.LittleEndian,
                    [
                        new PlatformSettingDefinition(
                            "write-conversion-report",
                            "Write Conversion Report",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "include-project-defined-preprocessor-symbols",
                            "Include Project Symbols",
                            PlatformSettingKind.Boolean,
                            "false",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "load-native-runtime-metadata",
                            "Load Native Runtime Metadata",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            [])
                    ])
            ],
            [
                new PlatformStorageProfileDefinition(
                    "homebrew-app",
                    "Homebrew App",
                    PlatformStorageProfileKind.LooseFiles,
                    "psp-homebrew-app",
                    allowContainerSegmentation: false)
            ],
            [
                new PlatformMediaProfileDefinition(
                    "psp-game-folder",
                    "PSP Game Folder",
                    PlatformMediaLayoutKind.InstallTree,
                    allowPhysicalDuplication: false,
                    preferLocalityOverDeduplication: true)
            ]);
    }
}
