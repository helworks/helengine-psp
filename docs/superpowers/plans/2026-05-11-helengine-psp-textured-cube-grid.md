# Helengine PSP Textured Cube Grid Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render the real city `textured_cube_grid` scene on PSP with distinct authored textures and the existing CPU directional-lighting path active at the same time.

**Architecture:** Keep the authored material and scene contract unchanged, preserve diffuse texture bindings in the existing runtime-material property path, and add a PSP-specific runtime texture/cache layer behind `RenderManager2D.BuildTextureFromRaw(...)`. Extend the PSP 3D renderer to preserve UVs, resolve `PspRuntimeTexture` from the runtime material, and submit GU textured vertices whose sampled texels are modulated by the existing CPU-computed lit vertex color.

**Tech Stack:** .NET 9, xUnit, C++17, PSPDEV/PSPSDK, PPSSPP, Docker, GNU Make, CMake

---

## File Structure

- Create: `builder.tests/CityTexturedCubeGridSceneTests.cs`
  Purpose: lock the authored city textured-cube scene and PSP material sidecar contract before touching runtime code.
- Modify: `builder.tests/PspPlatformAssetBuilderTests.cs`
  Purpose: prove the PSP builder stages imported cooked textures into the packaged game folder.
- Create: `src/platform/psp/rendering/PspRuntimeTexture.hpp`
  Purpose: store PSP-ready runtime texture data in a type the 3D renderer can bind directly.
- Create: `src/platform/psp/rendering/PspRuntimeTexture.cpp`
  Purpose: implement the PSP runtime texture accessors and pixel-buffer ownership.
- Create: `src/platform/psp/rendering/PspTextureCache.hpp`
  Purpose: own the PSP texture cache and the raw RGBA-to-ABGR conversion path.
- Create: `src/platform/psp/rendering/PspTextureCache.cpp`
  Purpose: build and cache `PspRuntimeTexture` instances from cooked `TextureAsset` payloads.
- Modify: `src/platform/psp/rendering/PspRenderManager2D.hpp`
  Purpose: make the PSP 2D runtime own the texture cache instead of returning metadata-only textures.
- Modify: `src/platform/psp/rendering/PspRenderManager2D.cpp`
  Purpose: route `BuildTextureFromRaw(...)` through the PSP texture cache.
- Modify: `src/platform/psp/rendering/PspRuntimeMaterial.hpp`
  Purpose: expose a typed PSP texture-resolution helper so the 3D renderer stays decoupled from generic property-block plumbing.
- Modify: `src/platform/psp/rendering/PspRuntimeMaterial.cpp`
  Purpose: resolve `PspRuntimeTexture` from the inherited runtime material texture bindings.
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`
  Purpose: preserve mesh UVs, bind textures, and submit textured-lit GU vertices for the full scene.
- Modify: `CMakeLists.txt`
  Purpose: compile the new PSP runtime texture and cache files.
- Modify: `README.md`
  Purpose: record that PSP now supports diffuse textures plus directional lighting with nearest sampling.

### Task 1: Lock The Textured Scene Contract Before Runtime Changes

**Files:**
- Create: `builder.tests/CityTexturedCubeGridSceneTests.cs`
- Modify: `builder.tests/PspPlatformAssetBuilderTests.cs`

- [ ] **Step 1: Write the failing city textured-scene regression file**

Create `builder.tests/CityTexturedCubeGridSceneTests.cs` with:

```csharp
using helengine.editor;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the authored city textured cube-grid scene stays compatible with the PSP textured-material contract.
/// </summary>
public sealed class CityTexturedCubeGridSceneTests {
    /// <summary>
    /// Absolute city project root used by the shared rendering-scene regression.
    /// </summary>
    const string CityProjectRootPath = @"C:\dev\helprojs\city";

    /// <summary>
    /// Relative authored material path for the first textured cube material.
    /// </summary>
    const string Cube00MaterialRelativePath = @"assets\Materials\rendering\textured_cube_grid\Cube00.helmat";

    /// <summary>
    /// Ensures the generated city textured cube material sidecar includes the PSP diffuse texture binding.
    /// </summary>
    [Fact]
    public void TexturedCubeGridMaterialSettings_include_psp_diffuse_texture_binding() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        string materialPath = Path.Combine(CityProjectRootPath, Cube00MaterialRelativePath);

        Assert.True(settingsService.TryLoad(materialPath, out AssetImportSettings settings));
        Assert.NotNull(settings);
        Assert.True(settings.Processor.Platforms.ContainsKey("psp"));
        Assert.Equal("standard-shader", settings.Processor.Platforms["psp"].Material.SchemaId);
        Assert.True(settings.Processor.Platforms["psp"].Material.FieldValues.ContainsKey("texture-id"));
        Assert.False(string.IsNullOrWhiteSpace(settings.Processor.Platforms["psp"].Material.FieldValues["texture-id"]));
    }

    /// <summary>
    /// Ensures the PSP material cook path preserves the authored diffuse texture asset id for the textured cube scene.
    /// </summary>
    [Fact]
    public void TexturedCubeGridMaterialCook_preserves_psp_diffuse_texture_asset_id() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        string materialPath = Path.Combine(CityProjectRootPath, Cube00MaterialRelativePath);

        Assert.True(settingsService.TryLoad(materialPath, out AssetImportSettings settings));
        Assert.NotNull(settings);
        Assert.True(settings.Processor.Platforms.ContainsKey("psp"));

        MaterialAsset sourceMaterialAsset = ReadTexturedCubeGridMaterialAsset();
        Assert.False(string.IsNullOrWhiteSpace(sourceMaterialAsset.DiffuseTextureAssetId));

        AssetPlatformProcessorSettings platformSettings = settings.Processor.Platforms["psp"];
        Dictionary<string, string> fieldValues = new Dictionary<string, string>(platformSettings.Material.FieldValues) {
            ["shader-asset-id"] = sourceMaterialAsset.ShaderAssetId,
            ["vertex-program"] = sourceMaterialAsset.VertexProgram,
            ["pixel-program"] = sourceMaterialAsset.PixelProgram,
            ["variant"] = sourceMaterialAsset.Variant
        };
        PspPlatformAssetBuilder builder = new PspPlatformAssetBuilder();
        PlatformMaterialCookResult cookResult = builder.CookMaterial(new PlatformMaterialCookRequest(
            Cube00MaterialRelativePath.Replace('\\', '/'),
            Cube00MaterialRelativePath.Replace('\\', '/'),
            "psp",
            "debug",
            "psp-forward",
            platformSettings.Material.SchemaId,
            fieldValues));

        MaterialAsset materialAsset = Assert.IsType<MaterialAsset>(AssetSerializer.DeserializeFromBytes(cookResult.CookedMaterialBytes));

        Assert.Equal(sourceMaterialAsset.DiffuseTextureAssetId, materialAsset.DiffuseTextureAssetId);
    }

    /// <summary>
    /// Reads the authored first textured-cube material asset from disk.
    /// </summary>
    /// <returns>Deserialized authored material asset.</returns>
    MaterialAsset ReadTexturedCubeGridMaterialAsset() {
        string materialPath = Path.Combine(CityProjectRootPath, Cube00MaterialRelativePath);
        Assert.True(File.Exists(materialPath));

        using FileStream stream = File.OpenRead(materialPath);
        return Assert.IsType<MaterialAsset>(EditorAssetBinarySerializer.Deserialize(stream));
    }
}
```

- [ ] **Step 2: Extend the packaged-build regression so imported textures must be staged**

In `builder.tests/PspPlatformAssetBuilderTests.cs`, update `BuildAsync_whenGivenGeneratedCoreAndCookedArtifacts_produces_psp_game_folder()` with these additions:

```csharp
        string importedTextureSourcePath = Path.Combine(sourceRoot, "cooked", "imported", "textures", "checker");

        Directory.CreateDirectory(Path.GetDirectoryName(importedTextureSourcePath)!);
        File.WriteAllText(importedTextureSourcePath, "texture payload");
```

Add the imported texture artifact to the `PlatformBuildManifest`:

```csharp
                    new PlatformBuildArtifact("cooked/imported/textures/checker", "texture:checker", "sha256:texture", "texture", "shared")
```

Then add the packaged-output assertion:

```csharp
            Assert.True(File.Exists(Path.Combine(outputRoot, "PSP", "GAME", "HELENGINE", "cooked", "imported", "textures", "checker")));
```

- [ ] **Step 3: Run the targeted textured builder regressions to confirm the authored contract is already green before runtime work**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~TexturedCubeGrid" -v minimal`

Expected: PASS. The diffuse-texture cook and staging contract should already be valid before any PSP runtime texturing code is added, and this step locks that in explicitly.

- [ ] **Step 4: Run the full PSP builder test suite after adding the regressions**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS with the new textured-scene regressions and the imported-texture staging assertion green.

- [ ] **Step 5: Commit the textured-scene contract coverage**

```bash
git add builder.tests/CityTexturedCubeGridSceneTests.cs builder.tests/PspPlatformAssetBuilderTests.cs
git commit -m "Add PSP textured scene regressions"
```

### Task 2: Add A PSP Runtime Texture Type And Cache Behind RenderManager2D

**Files:**
- Create: `src/platform/psp/rendering/PspRuntimeTexture.hpp`
- Create: `src/platform/psp/rendering/PspRuntimeTexture.cpp`
- Create: `src/platform/psp/rendering/PspTextureCache.hpp`
- Create: `src/platform/psp/rendering/PspTextureCache.cpp`
- Modify: `src/platform/psp/rendering/PspRenderManager2D.hpp`
- Modify: `src/platform/psp/rendering/PspRenderManager2D.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the PSP runtime texture type**

Create `src/platform/psp/rendering/PspRuntimeTexture.hpp` with:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "RuntimeTexture.hpp"

namespace helengine::psp::rendering {
    /// Stores PSP-ready ABGR8888 texture pixels that can be bound directly through GU.
    class PspRuntimeTexture final : public RuntimeTexture {
    public:
        /// Creates an empty PSP runtime texture.
        PspRuntimeTexture();

        /// Gets the PSP-ready ABGR8888 texel pointer, or <c>nullptr</c> when the texture has no pixels.
        const std::uint32_t* GetPixelsAbgr8888() const;

        /// Gets the number of stored ABGR8888 texels.
        std::size_t GetPixelCount() const;

        /// Gets whether the texture owns pixel data.
        bool HasPixels() const;

        /// Replaces the owned ABGR8888 pixel buffer.
        void SetPixelsAbgr8888(std::vector<std::uint32_t>&& pixels);

    private:
        /// Stores PSP-ready ABGR8888 texels in row-major order.
        std::vector<std::uint32_t> PixelsAbgr8888;
    };
}
```

Create `src/platform/psp/rendering/PspRuntimeTexture.cpp` with:

```cpp
#include "platform/psp/rendering/PspRuntimeTexture.hpp"

#include <utility>

namespace helengine::psp::rendering {
    /// Creates an empty PSP runtime texture.
    PspRuntimeTexture::PspRuntimeTexture() = default;

    /// Gets the PSP-ready ABGR8888 texel pointer, or <c>nullptr</c> when the texture has no pixels.
    const std::uint32_t* PspRuntimeTexture::GetPixelsAbgr8888() const {
        return PixelsAbgr8888.empty()
            ? nullptr
            : PixelsAbgr8888.data();
    }

    /// Gets the number of stored ABGR8888 texels.
    std::size_t PspRuntimeTexture::GetPixelCount() const {
        return PixelsAbgr8888.size();
    }

    /// Gets whether the texture owns pixel data.
    bool PspRuntimeTexture::HasPixels() const {
        return !PixelsAbgr8888.empty();
    }

    /// Replaces the owned ABGR8888 pixel buffer.
    void PspRuntimeTexture::SetPixelsAbgr8888(std::vector<std::uint32_t>&& pixels) {
        PixelsAbgr8888 = std::move(pixels);
    }
}
```

- [ ] **Step 2: Create the PSP texture cache and RGBA conversion path**

Create `src/platform/psp/rendering/PspTextureCache.hpp` with:

```cpp
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "RuntimeTexture.hpp"
#include "TextureAsset.hpp"

namespace helengine::psp::rendering {
    class PspRuntimeTexture;

    /// Builds and caches PSP runtime textures from cooked raw texture assets.
    class PspTextureCache final {
    public:
        /// Builds one PSP runtime texture from the cooked texture asset, reusing a cached instance when possible.
        RuntimeTexture* BuildTextureFromRaw(TextureAsset* data);

    private:
        /// Creates one uncached PSP runtime texture from the cooked texture payload.
        PspRuntimeTexture* CreateTexture(TextureAsset* data);

        /// Converts raw RGBA bytes into PSP-ready ABGR8888 texels.
        static std::vector<std::uint32_t> ConvertRgbaBytesToAbgr8888(TextureAsset* data);

        /// Stores cached PSP runtime textures by cooked texture asset id.
        std::unordered_map<std::string, PspRuntimeTexture*> CachedTextures;
    };
}
```

Create `src/platform/psp/rendering/PspTextureCache.cpp` with:

```cpp
#include "platform/psp/rendering/PspTextureCache.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "platform/psp/rendering/PspRuntimeTexture.hpp"

namespace helengine::psp::rendering {
    namespace {
        /// Packs one raw RGBA texel into the ABGR8888 layout expected by the PSP texture unit.
        std::uint32_t PackRgbaToAbgr(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
            return (static_cast<std::uint32_t>(alpha) << 24)
                | (static_cast<std::uint32_t>(blue) << 16)
                | (static_cast<std::uint32_t>(green) << 8)
                | static_cast<std::uint32_t>(red);
        }
    }

    /// Builds one PSP runtime texture from the cooked texture asset, reusing a cached instance when possible.
    RuntimeTexture* PspTextureCache::BuildTextureFromRaw(TextureAsset* data) {
        if (data == nullptr) {
            throw std::invalid_argument("PSP runtime texture data is required.");
        }

        const std::string cacheKey = data->get_Id();
        if (!cacheKey.empty()) {
            auto cachedTextureIterator = CachedTextures.find(cacheKey);
            if (cachedTextureIterator != CachedTextures.end()) {
                return cachedTextureIterator->second;
            }
        }

        PspRuntimeTexture* runtimeTexture = CreateTexture(data);
        if (!cacheKey.empty()) {
            CachedTextures.emplace(cacheKey, runtimeTexture);
        }

        return runtimeTexture;
    }

    /// Creates one uncached PSP runtime texture from the cooked texture payload.
    PspRuntimeTexture* PspTextureCache::CreateTexture(TextureAsset* data) {
        if (data == nullptr) {
            throw std::invalid_argument("PSP runtime texture data is required.");
        } else if (data->Colors == nullptr) {
            throw std::runtime_error("PSP runtime texture data must include RGBA color bytes.");
        }

        const std::size_t expectedByteCount = static_cast<std::size_t>(data->Width) * static_cast<std::size_t>(data->Height) * 4u;
        if (static_cast<std::size_t>(data->Colors->Length) != expectedByteCount) {
            throw std::runtime_error("PSP runtime texture data length does not match width * height * 4.");
        }

        PspRuntimeTexture* runtimeTexture = new PspRuntimeTexture();
        runtimeTexture->set_Id(data->get_Id());
        runtimeTexture->set_Width(data->Width);
        runtimeTexture->set_Height(data->Height);
        runtimeTexture->SetPixelsAbgr8888(ConvertRgbaBytesToAbgr8888(data));
        return runtimeTexture;
    }

    /// Converts raw RGBA bytes into PSP-ready ABGR8888 texels.
    std::vector<std::uint32_t> PspTextureCache::ConvertRgbaBytesToAbgr8888(TextureAsset* data) {
        const std::size_t pixelCount = static_cast<std::size_t>(data->Width) * static_cast<std::size_t>(data->Height);
        std::vector<std::uint32_t> pixels;
        pixels.reserve(pixelCount);

        for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
            const std::size_t byteOffset = pixelIndex * 4u;
            pixels.push_back(PackRgbaToAbgr(
                static_cast<std::uint8_t>(data->Colors->Data[byteOffset]),
                static_cast<std::uint8_t>(data->Colors->Data[byteOffset + 1u]),
                static_cast<std::uint8_t>(data->Colors->Data[byteOffset + 2u]),
                static_cast<std::uint8_t>(data->Colors->Data[byteOffset + 3u])));
        }

        return pixels;
    }
}
```

- [ ] **Step 3: Route PSP texture creation through the cache**

In `src/platform/psp/rendering/PspRenderManager2D.hpp`, add the cache include and field:

```cpp
#include "platform/psp/rendering/PspTextureCache.hpp"
```

```cpp
    private:
        /// Stores cached PSP runtime textures built from cooked texture assets.
        PspTextureCache TextureCache;
```

Replace `BuildTextureFromRaw(...)` in `src/platform/psp/rendering/PspRenderManager2D.cpp` with:

```cpp
    /// Builds a PSP runtime texture from raw texture data and reuses cached instances by asset id.
    RuntimeTexture* PspRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        return TextureCache.BuildTextureFromRaw(data);
    }
```

- [ ] **Step 4: Add the new PSP texture sources to the native build**

In `CMakeLists.txt`, append these entries to `HELENGINE_PSP_SOURCES` immediately before `src/platform/psp/rendering/PspRuntimeMaterial.cpp`:

```cmake
    src/platform/psp/rendering/PspRuntimeTexture.cpp
    src/platform/psp/rendering/PspTextureCache.cpp
```

- [ ] **Step 5: Rebuild the PSP player to verify the runtime texture/cache layer compiles**

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& {
    $buildRoot = Get-ChildItem 'C:\Users\Helena\AppData\Local\Temp\helengine-platform-build\psp' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $generatedCoreRoot = ($buildRoot.FullName + '\generated-core')
    docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v ($generatedCoreRoot -replace '\\','/'):/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
}"
```

Expected: PASS with `build/EBOOT.PBP` rebuilt and no missing-source or missing-include errors.

- [ ] **Step 6: Commit the PSP runtime texture cache**

```bash
git add CMakeLists.txt src/platform/psp/rendering/PspRuntimeTexture.hpp src/platform/psp/rendering/PspRuntimeTexture.cpp src/platform/psp/rendering/PspTextureCache.hpp src/platform/psp/rendering/PspTextureCache.cpp src/platform/psp/rendering/PspRenderManager2D.hpp src/platform/psp/rendering/PspRenderManager2D.cpp
git commit -m "Add PSP runtime texture cache"
```

### Task 3: Combine GU Texturing With The Existing CPU Directional-Lighting Path

**Files:**
- Modify: `src/platform/psp/rendering/PspRuntimeMaterial.hpp`
- Modify: `src/platform/psp/rendering/PspRuntimeMaterial.cpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`

- [ ] **Step 1: Add a typed PSP texture-resolution helper on the runtime material**

In `src/platform/psp/rendering/PspRuntimeMaterial.hpp`, add:

```cpp
#include "platform/psp/rendering/PspRuntimeTexture.hpp"
```

Then add this method declaration above `LoadFromCooked(...)`:

```cpp
        /// Resolves the first bound PSP runtime texture when the material exposes one.
        bool TryResolveTexture(PspRuntimeTexture*& texture);
```

Implement it in `src/platform/psp/rendering/PspRuntimeMaterial.cpp`:

```cpp
    /// Resolves the first bound PSP runtime texture when the material exposes one.
    bool PspRuntimeMaterial::TryResolveTexture(PspRuntimeTexture*& texture) {
        RuntimeTexture* resolvedTexture = ResolveTexture();
        if (resolvedTexture == nullptr) {
            texture = nullptr;
            return false;
        }

        texture = dynamic_cast<PspRuntimeTexture*>(resolvedTexture);
        if (texture == nullptr) {
            throw std::runtime_error("PSP textured materials require PspRuntimeTexture instances.");
        }

        return true;
    }
```

- [ ] **Step 2: Preserve mesh UVs and add a GU textured vertex layout**

In `src/platform/psp/rendering/PspRenderManager3D.cpp`, add these includes:

```cpp
#include <pspkernel.h>

#include "float2.hpp"
#include "platform/psp/rendering/PspRuntimeTexture.hpp"
```

Extend the internal mesh and vertex structs:

```cpp
        struct PspMeshRecord {
            std::vector<float3> Positions;
            std::vector<float3> Normals;
            std::vector<float2> TexCoords;
            std::vector<std::uint32_t> Indices;
        };

        struct PspTexturedLitVertex {
            float U;
            float V;
            std::uint32_t Color;
            float X;
            float Y;
            float Z;
        };
```

Then update `BuildModelFromRaw(...)` so it preserves UVs:

```cpp
            if (data->TexCoords != nullptr && data->TexCoords->Length > 0) {
                record.TexCoords.reserve(static_cast<std::size_t>(data->TexCoords->Length));
                for (int32_t index = 0; index < data->TexCoords->Length; index++) {
                    record.TexCoords.push_back((*data->TexCoords)[index]);
                }
            }
```

- [ ] **Step 3: Add one helper that binds a PSP runtime texture with modulate + nearest sampling**

Still inside the anonymous namespace in `src/platform/psp/rendering/PspRenderManager3D.cpp`, add:

```cpp
        /// Binds one PSP runtime texture for GU sampling or disables texturing when no texture exists.
        void BindTexture(PspRuntimeTexture* texture) {
            if (texture == nullptr || !texture->HasPixels()) {
                sceGuDisable(GU_TEXTURE_2D);
                return;
            }

            sceKernelDcacheWritebackRange(
                const_cast<std::uint32_t*>(texture->GetPixelsAbgr8888()),
                static_cast<unsigned int>(texture->GetPixelCount() * sizeof(std::uint32_t)));

            sceGuEnable(GU_TEXTURE_2D);
            sceGuTexMode(GU_PSM_8888, 0, 0, 0);
            sceGuTexImage(0, texture->get_Width(), texture->get_Height(), texture->get_Width(), texture->GetPixelsAbgr8888());
            sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
            sceGuTexFilter(GU_NEAREST, GU_NEAREST);
            sceGuTexWrap(GU_REPEAT, GU_REPEAT);
        }
```

- [ ] **Step 4: Replace the per-draw submission path so textured materials use UVs plus lit vertex colors**

In `Visit(IDrawable3D* drawable)`, keep the existing world-matrix and CPU-lighting code, then replace the vertex-submission block with:

```cpp
        PspRuntimeTexture* texture = nullptr;
        const bool hasTexture = pspRuntimeMaterial->TryResolveTexture(texture);
        if (hasTexture && record.TexCoords.empty()) {
            throw std::runtime_error("Textured PSP drawables require mesh texcoords.");
        }

        BindTexture(texture);

        if (hasTexture) {
            PspTexturedLitVertex* vertices = static_cast<PspTexturedLitVertex*>(sceGuGetMemory(sizeof(PspTexturedLitVertex) * static_cast<std::size_t>(vertexCount)));
            for (int32_t index = 0; index < vertexCount; index++) {
                std::uint32_t sourceIndex = record.Indices.empty()
                    ? static_cast<std::uint32_t>(index)
                    : record.Indices[static_cast<std::size_t>(index)];
                if (sourceIndex >= record.Positions.size() || sourceIndex >= record.TexCoords.size()) {
                    throw std::runtime_error("PSP textured draw submitted an out-of-range mesh index.");
                }

                const float3& position = record.Positions[sourceIndex];
                const float3 sourceNormal = sourceIndex < record.Normals.size()
                    ? record.Normals[sourceIndex]
                    : float3(0.0f, 0.0f, 1.0f);
                const float3 worldNormal = float3::Normalize(RotateNormal(sourceNormal, drawable->get_Parent()));
                const float2& texCoord = record.TexCoords[sourceIndex];

                float4 litColor = baseColor;
                if (useLighting) {
                    const float ambient = LightingSettings.AmbientIntensity;
                    float diffuse = 0.0f;
                    if (CurrentLighting.HasDirectionalLight) {
                        const float3 lightDirection = float3::Normalize(CurrentLighting.DirectionalLightDirection);
                        diffuse = std::max(0.0f, float3::Dot(worldNormal, lightDirection));
                    }

                    litColor = ClampColor(float4(
                        baseColor.X * (ambient + (diffuse * CurrentLighting.DirectionalLightColor.X * CurrentLighting.DirectionalLightIntensity)),
                        baseColor.Y * (ambient + (diffuse * CurrentLighting.DirectionalLightColor.Y * CurrentLighting.DirectionalLightIntensity)),
                        baseColor.Z * (ambient + (diffuse * CurrentLighting.DirectionalLightColor.Z * CurrentLighting.DirectionalLightIntensity)),
                        baseColor.W));
                }

                vertices[index] = PspTexturedLitVertex {
                    texCoord.X,
                    texCoord.Y,
                    ConvertColorToAbgr(litColor),
                    position.X,
                    position.Y,
                    position.Z
                };
            }

            sceGumDrawArray(
                GU_TRIANGLES,
                GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                vertexCount,
                nullptr,
                vertices);
            return;
        }

        PspLitVertex* vertices = static_cast<PspLitVertex*>(sceGuGetMemory(sizeof(PspLitVertex) * static_cast<std::size_t>(vertexCount)));
        for (int32_t index = 0; index < vertexCount; index++) {
            std::uint32_t sourceIndex = record.Indices.empty()
                ? static_cast<std::uint32_t>(index)
                : record.Indices[static_cast<std::size_t>(index)];
            if (sourceIndex >= record.Positions.size()) {
                return;
            }

            const float3& position = record.Positions[sourceIndex];
            const float3 sourceNormal = sourceIndex < record.Normals.size()
                ? record.Normals[sourceIndex]
                : float3(0.0f, 0.0f, 1.0f);
            const float3 worldNormal = float3::Normalize(RotateNormal(sourceNormal, drawable->get_Parent()));

            float4 litColor = baseColor;
            if (useLighting) {
                const float ambient = LightingSettings.AmbientIntensity;
                float diffuse = 0.0f;
                if (CurrentLighting.HasDirectionalLight) {
                    const float3 lightDirection = float3::Normalize(CurrentLighting.DirectionalLightDirection);
                    diffuse = std::max(0.0f, float3::Dot(worldNormal, lightDirection));
                }

                litColor = ClampColor(float4(
                    baseColor.X * (ambient + (diffuse * CurrentLighting.DirectionalLightColor.X * CurrentLighting.DirectionalLightIntensity)),
                    baseColor.Y * (ambient + (diffuse * CurrentLighting.DirectionalLightColor.Y * CurrentLighting.DirectionalLightIntensity)),
                    baseColor.Z * (ambient + (diffuse * CurrentLighting.DirectionalLightColor.Z * CurrentLighting.DirectionalLightIntensity)),
                    baseColor.W));
            }

            vertices[index] = PspLitVertex {
                ConvertColorToAbgr(litColor),
                position.X,
                position.Y,
                position.Z
            };
        }

        sceGumDrawArray(
            GU_TRIANGLES,
            GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
            vertexCount,
            nullptr,
            vertices);
```

- [ ] **Step 5: Rebuild the PSP player after the texture+lighting renderer changes**

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& {
    $buildRoot = Get-ChildItem 'C:\Users\Helena\AppData\Local\Temp\helengine-platform-build\psp' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $generatedCoreRoot = ($buildRoot.FullName + '\generated-core')
    docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v ($generatedCoreRoot -replace '\\','/'):/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
}"
```

Expected: PASS with the textured vertex layout and new runtime texture types compiling cleanly.

- [ ] **Step 6: Commit the PSP textured directional renderer**

```bash
git add src/platform/psp/rendering/PspRuntimeMaterial.hpp src/platform/psp/rendering/PspRuntimeMaterial.cpp src/platform/psp/rendering/PspRenderManager3D.cpp
git commit -m "Add PSP textured directional rendering"
```

### Task 4: Export The Full City Scene, Verify It In PPSSPP, And Document The Milestone

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update the PSP README milestone text**

Replace the PSP rendering-status section in `README.md` with:

````md
## Current milestone

- Docker-only build using the official PSPDEV SDK image
- Standard PSP homebrew packaging ending in `EBOOT.PBP`
- Generated Helengine runtime sources bundled into the PSP executable
- Runtime startup reaches the real startup scene and main loop
- PSP 3D rendering supports base-color materials and diffuse textures under CPU directional lighting
- Diffuse textures currently use GU nearest sampling with no mipmaps
- Ambient lighting currently defaults to `0.25` from PSP renderer settings

## Rendering status

The PSP renderer currently supports:

- live scene camera traversal
- mesh submission from cooked runtime scene data
- base-color materials
- diffuse texture sampling
- directional-light ambient plus diffuse shading
- unlit material bypass

The PSP renderer does not yet support:

- mipmaps
- filtered sampling beyond nearest
- point lights
- spot lights
- specular
- emissive
- shadows
- fixed-function GU lighting backend
````

- [ ] **Step 2: Re-run the full PSP builder test suite before the vertical-scene verification**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS.

- [ ] **Step 3: Export the full city PSP build for the textured cube-grid scene**

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& {
    $env:HELENGINE_PSP_REPOSITORY_ROOT = 'C:\dev\helworks\helengine-psp'
    dotnet 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll' --project 'C:\dev\helprojs\city' --build psp --output 'C:\tmp\city-psp-textured-cube-grid-build'
}"
```

Expected:

- `C:\tmp\city-psp-textured-cube-grid-build\PSP\GAME\HELENGINE\cooked\scenes\rendering\textured_cube_grid.hasset` exists
- `C:\tmp\city-psp-textured-cube-grid-build\PSP\GAME\HELENGINE\cooked\imported\...` contains the generated cube textures

- [ ] **Step 4: Install the rebuilt executable and the exported city cooked payload into PPSSPP**

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& {
    Copy-Item 'C:\dev\helworks\helengine-psp\build\EBOOT.PBP' 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP' -Force
    Copy-Item 'C:\tmp\city-psp-textured-cube-grid-build\PSP\GAME\HELENGINE\cooked\*' 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\cooked' -Recurse -Force
}"
```

Expected: PASS with the PPSSPP memstick tree now containing the latest `EBOOT.PBP`, `textured_cube_grid` scene, and imported cooked textures.

- [ ] **Step 5: Launch PPSSPP and verify the real textured scene**

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "Start-Process 'C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe' -ArgumentList 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP' -WindowStyle Hidden"
```

Then verify:

- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log` reaches `Stage complete RuntimeMainLoop`
- the on-screen scene is `textured_cube_grid`
- the sixteen cubes are textured, not flat white
- directional lighting is still visible across the textured cube faces
- sampling is nearest, with no mip or filtered smoothing

- [ ] **Step 6: Commit the milestone documentation**

```bash
git add README.md
git commit -m "Document PSP textured lighting milestone"
```
