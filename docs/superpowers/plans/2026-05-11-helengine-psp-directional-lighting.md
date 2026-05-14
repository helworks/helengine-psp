# Helengine PSP Directional Lighting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add scene-driven PSP directional lighting with a renderer-owned ambient default, CPU vertex Lambert shading, and a runtime contract that preserves future fixed-function lighting support without changing cooked asset semantics.

**Architecture:** Extend the PSP material cook path so cooked materials preserve lighting intent instead of collapsing to a flat color, then add a PSP runtime-material layer and renderer lighting settings that separate scene parsing from backend execution. Implement the first backend as CPU vertex lighting driven by live directional-light scene components, while reserving an explicit fixed-function pipeline enum and switch so future GU lighting can reuse the same cooked assets and runtime material contract.

**Tech Stack:** .NET 9, xUnit, C++17, PSPDEV/PSPSDK, PPSSPP, Docker, GNU Make, CMake

---

## File Structure

- Modify: `builder.tests/PspPlatformAssetBuilderTests.cs`
  Purpose: lock the PSP material schema and cooked material payload so lighting intent is preserved end to end.
- Modify: `builder/PspPlatformDefinitionFactory.cs`
  Purpose: expose PSP lighting-related standard material fields to the editor build graph.
- Modify: `builder/PspPlatformAssetBuilder.cs`
  Purpose: encode the PSP lighting contract into cooked material constant buffers without changing the generic `MaterialAsset` transport.
- Create: `src/platform/psp/rendering/PspLightingPipeline.hpp`
  Purpose: define the PSP lighting backend choice independently from material semantics.
- Create: `src/platform/psp/rendering/PspMaterialLightingResponse.hpp`
  Purpose: define the per-material lighting behavior for PSP runtime materials.
- Create: `src/platform/psp/rendering/PspLightingSettings.hpp`
  Purpose: store renderer-owned ambient and backend-selection defaults.
- Create: `src/platform/psp/rendering/PspSceneLightingSnapshot.hpp`
  Purpose: store the per-frame directional-light data resolved from live scene components.
- Create: `src/platform/psp/rendering/PspRuntimeMaterial.hpp`
  Purpose: expose a PSP runtime material with base color and lighting intent instead of relying on bare `RuntimeMaterial`.
- Create: `src/platform/psp/rendering/PspRuntimeMaterial.cpp`
  Purpose: decode cooked `MaterialAsset` constant buffers into PSP runtime-material state.
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`
  Purpose: declare the PSP lighting settings and scene-light resolution helpers used by the 3D renderer.
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`
  Purpose: preserve normals, resolve scene directional lights, compute CPU vertex Lambert shading, and submit per-vertex colors through GU.
- Modify: `CMakeLists.txt`
  Purpose: compile the new PSP lighting files.
- Modify: `README.md`
  Purpose: update the PSP milestone description from unlit cube rendering to directional-light directional-scene rendering and record the ambient default.

### Task 1: Preserve PSP Lighting Intent In The Material Schema And Cooked Payload

**Files:**
- Modify: `builder.tests/PspPlatformAssetBuilderTests.cs`
- Modify: `builder/PspPlatformDefinitionFactory.cs`
- Modify: `builder/PspPlatformAssetBuilder.cs`

- [ ] **Step 1: Write the failing builder tests for PSP lighting schema and cooked payload preservation**

Insert these tests into `builder.tests/PspPlatformAssetBuilderTests.cs` after `Descriptor_and_definition_expose_standard_material_fields`:

```csharp
    /// <summary>
    /// Verifies the PSP material schema exposes lighting intent fields that stay independent from the renderer backend.
    /// </summary>
    [Fact]
    public void Descriptor_and_definition_expose_psp_lighting_fields() {
        PspPlatformAssetBuilder builder = new();

        PlatformMaterialSchemaDefinition schema = Assert.Single(
            builder.Definition.MaterialSchemas,
            materialSchema => materialSchema.SchemaId == "standard-shader");

        Assert.Contains(schema.Fields, field =>
            field.FieldId == "lighting-response"
            && field.FieldKind == PlatformMaterialFieldKind.Text
            && field.DefaultValue == "lit-directional");
        Assert.Contains(schema.Fields, field =>
            field.FieldId == "receives-lighting"
            && field.FieldKind == PlatformMaterialFieldKind.Boolean
            && field.DefaultValue == "true");
    }

    /// <summary>
    /// Verifies the PSP material cook path preserves lighting intent in a dedicated constant buffer.
    /// </summary>
    [Fact]
    public void CookMaterial_preserves_psp_lighting_configuration() {
        PspPlatformAssetBuilder builder = new();

        PlatformMaterialCookResult result = builder.CookMaterial(new PlatformMaterialCookRequest(
            "Materials/Test.helmat",
            "Materials/Test.helmat",
            "psp",
            "debug",
            "psp-forward",
            "standard-shader",
            new Dictionary<string, string> {
                ["use-custom-shader"] = "false",
                ["shader-asset-id"] = "engine:material:standard",
                ["vertex-program"] = "ForwardStandard.vs",
                ["pixel-program"] = "ForwardStandard.ps",
                ["variant"] = "Mesh",
                ["base-color"] = "#336699",
                ["lighting-response"] = "unlit",
                ["receives-lighting"] = "false",
                ["texture-id"] = string.Empty,
                ["casts-shadow"] = "false",
                ["receives-shadow"] = "true"
            }));

        MaterialAsset materialAsset = Assert.IsType<MaterialAsset>(AssetSerializer.DeserializeFromBytes(result.CookedMaterialBytes));
        MaterialConstantBufferAsset lightingBuffer = Assert.Single(materialAsset.ConstantBuffers, buffer => buffer.Name == "LightingConfigBuffer");

        Assert.Equal(16, lightingBuffer.Data.Length);
        Assert.Equal(0.0f, BitConverter.ToSingle(lightingBuffer.Data, 0));
        Assert.Equal(0.0f, BitConverter.ToSingle(lightingBuffer.Data, 4));
        Assert.Equal(0.0f, BitConverter.ToSingle(lightingBuffer.Data, 8));
        Assert.Equal(0.0f, BitConverter.ToSingle(lightingBuffer.Data, 12));
    }
```

- [ ] **Step 2: Run the targeted builder tests to verify the new coverage fails**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~psp_lighting" -v minimal`

Expected: FAIL because the current PSP material schema does not expose `lighting-response` or `receives-lighting`, and `CookMaterial(...)` does not emit `LightingConfigBuffer`.

- [ ] **Step 3: Add the PSP lighting fields to the standard material schema**

In `builder/PspPlatformDefinitionFactory.cs`, insert these field definitions immediately after the existing `base-color` field:

```csharp
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
```

Keep `texture-id`, `casts-shadow`, and `receives-shadow` after these new fields so the PSP material schema remains stable and readable.

- [ ] **Step 4: Encode PSP lighting intent into the cooked material payload**

In `builder/PspPlatformAssetBuilder.cs`, add these constants near the existing material-field constants:

```csharp
    /// <summary>
    /// Stable material field identifier used for the PSP lighting response mode.
    /// </summary>
    const string LightingResponseFieldId = "lighting-response";

    /// <summary>
    /// Stable material field identifier used for the PSP receives-lighting flag.
    /// </summary>
    const string ReceivesLightingFieldId = "receives-lighting";

    /// <summary>
    /// Constant-buffer name used for the PSP lighting configuration payload.
    /// </summary>
    const string LightingConfigBufferName = "LightingConfigBuffer";
```

Add these helper methods near the existing parsing helpers:

```csharp
    /// <summary>
    /// Resolves the cooked numeric code used by the PSP runtime material lighting-response contract.
    /// </summary>
    /// <param name="serializedLightingResponse">Serialized lighting response value.</param>
    /// <returns>Cooked numeric response code.</returns>
    static float ParseLightingResponseCode(string serializedLightingResponse) {
        string normalized = string.IsNullOrWhiteSpace(serializedLightingResponse)
            ? "lit-directional"
            : serializedLightingResponse.Trim().ToLowerInvariant();

        if (normalized == "unlit") {
            return 0.0f;
        } else if (normalized == "lit-directional") {
            return 1.0f;
        }

        throw new InvalidOperationException("Lighting response must be 'unlit' or 'lit-directional'.");
    }

    /// <summary>
    /// Packs the PSP lighting configuration into one 16-byte constant-buffer payload.
    /// </summary>
    /// <param name="receivesLighting">Whether the material receives scene lighting.</param>
    /// <param name="lightingResponseCode">Cooked numeric lighting-response code.</param>
    /// <returns>Packed lighting configuration bytes.</returns>
    static byte[] CreateLightingConfigConstantBufferData(bool receivesLighting, float lightingResponseCode) {
        using MemoryStream stream = new MemoryStream();
        using EngineBinaryWriter writer = EngineBinaryWriter.Create(stream, EngineBinaryEndianness.LittleEndian);
        writer.WriteSingle(receivesLighting ? 1.0f : 0.0f);
        writer.WriteSingle(lightingResponseCode);
        writer.WriteSingle(0.0f);
        writer.WriteSingle(0.0f);
        return stream.ToArray();
    }
```

Then update `CookMaterial(...)` so it reads the new fields and appends the new constant buffer:

```csharp
        string lightingResponse = request.FieldValues.TryGetValue(LightingResponseFieldId, out string authoredLightingResponse) && !string.IsNullOrWhiteSpace(authoredLightingResponse)
            ? authoredLightingResponse
            : "lit-directional";
        bool receivesLighting = ReadOptionalBooleanField(request.FieldValues, ReceivesLightingFieldId, true);
        float lightingResponseCode = ParseLightingResponseCode(lightingResponse);

        MaterialAsset materialAsset = new MaterialAsset {
            Id = request.MaterialAssetId,
            ShaderAssetId = shaderAssetId,
            VertexProgram = vertexProgram,
            PixelProgram = pixelProgram,
            Variant = variant,
            DiffuseTextureAssetId = diffuseTextureAssetId,
            CastsShadows = castsShadows,
            ReceivesShadows = receivesShadows,
            RenderState = new MaterialRenderState(),
            ConstantBuffers = [
                new MaterialConstantBufferAsset {
                    Name = BaseColorBufferName,
                    Data = CreateFloat4ConstantBufferData(ParseBaseColor(baseColor))
                },
                new MaterialConstantBufferAsset {
                    Name = LightingConfigBufferName,
                    Data = CreateLightingConfigConstantBufferData(receivesLighting, lightingResponseCode)
                }
            ]
        };
```

- [ ] **Step 5: Run the full PSP builder test suite to verify the material contract is green**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS with the new schema and cooked-material assertions green.

- [ ] **Step 6: Commit the PSP material lighting contract**

```bash
git add builder.tests/PspPlatformAssetBuilderTests.cs builder/PspPlatformDefinitionFactory.cs builder/PspPlatformAssetBuilder.cs
git commit -m "Add PSP material lighting contract"
```

### Task 2: Add PSP Runtime Material And Lighting Types Without Changing Scene Semantics

**Files:**
- Create: `src/platform/psp/rendering/PspLightingPipeline.hpp`
- Create: `src/platform/psp/rendering/PspMaterialLightingResponse.hpp`
- Create: `src/platform/psp/rendering/PspLightingSettings.hpp`
- Create: `src/platform/psp/rendering/PspSceneLightingSnapshot.hpp`
- Create: `src/platform/psp/rendering/PspRuntimeMaterial.hpp`
- Create: `src/platform/psp/rendering/PspRuntimeMaterial.cpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Reproduce the current visual red state in PPSSPP with the directional-light test scene**

Run the current PSP build with the current cube directional-light payload installed to:

- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP`
- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\cooked\scenes\rendering\cube_test.hasset`
- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\cooked\engine\models\cube.hasset`
- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\cooked\engine\materials\standard.hasset`
- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\cooked\shaders\ForwardStandardShader.dx11.hasset`

Launch: `C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe`

Expected: the scene still boots, but the lit geometry remains flat white because PSP runtime materials and scene lighting are not wired yet.

- [ ] **Step 2: Add the PSP lighting-backend and material-response enums**

Create `src/platform/psp/rendering/PspLightingPipeline.hpp` with:

```cpp
#pragma once

namespace helengine::psp::rendering {
    /// Selects the PSP lighting backend used by the 3D renderer.
    enum class PspLightingPipeline {
        CpuVertexLambert = 0,
        FixedFunctionLambert = 1
    };
}
```

Create `src/platform/psp/rendering/PspMaterialLightingResponse.hpp` with:

```cpp
#pragma once

namespace helengine::psp::rendering {
    /// Describes how one PSP runtime material should react to scene lighting.
    enum class PspMaterialLightingResponse {
        Unlit = 0,
        LitDirectional = 1
    };
}
```

- [ ] **Step 3: Add the renderer-owned PSP lighting settings and per-frame scene-light snapshot**

Create `src/platform/psp/rendering/PspLightingSettings.hpp` with:

```cpp
#pragma once

#include "platform/psp/rendering/PspLightingPipeline.hpp"

namespace helengine::psp::rendering {
    /// Stores renderer-owned PSP lighting defaults and backend choices.
    struct PspLightingSettings {
        /// Default ambient intensity used until ambient becomes scene-authored.
        float AmbientIntensity = 0.25f;

        /// Active PSP lighting pipeline.
        PspLightingPipeline Pipeline = PspLightingPipeline::CpuVertexLambert;
    };
}
```

Create `src/platform/psp/rendering/PspSceneLightingSnapshot.hpp` with:

```cpp
#pragma once

#include "float3.hpp"
#include "float4.hpp"

namespace helengine::psp::rendering {
    /// Stores the live scene-light data resolved for one PSP camera render pass.
    struct PspSceneLightingSnapshot {
        /// Whether a usable directional light was found in the scene.
        bool HasDirectionalLight = false;

        /// World-space light direction used for diffuse lighting.
        float3 DirectionalLightDirection = float3(0.0f, 0.0f, -1.0f);

        /// Authored directional-light color.
        float4 DirectionalLightColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

        /// Authored directional-light intensity.
        float DirectionalLightIntensity = 1.0f;
    };
}
```

- [ ] **Step 4: Add a PSP runtime material that decodes cooked lighting intent**

Create `src/platform/psp/rendering/PspRuntimeMaterial.hpp` with:

```cpp
#pragma once

#include <cstdint>

#include "MaterialAsset.hpp"
#include "RuntimeMaterial.hpp"
#include "platform/psp/rendering/PspMaterialLightingResponse.hpp"
#include "float4.hpp"

namespace helengine::psp::rendering {
    /// Stores PSP-specific runtime material state derived from the cooked material payload.
    class PspRuntimeMaterial final : public RuntimeMaterial {
    public:
        /// Creates one PSP runtime material with lit-directional defaults.
        PspRuntimeMaterial();

        /// Gets the authored base color used by the PSP renderer.
        const float4& GetBaseColor() const;

        /// Gets whether the material receives scene lighting.
        bool GetReceivesLighting() const;

        /// Gets the PSP lighting-response mode.
        PspMaterialLightingResponse GetLightingResponse() const;

        /// Loads PSP material state from one cooked material asset.
        void LoadFromCooked(MaterialAsset* materialAsset);

    private:
        /// Stores the authored base color.
        float4 BaseColor;

        /// Stores whether the material receives scene lighting.
        bool ReceivesLighting;

        /// Stores the PSP lighting-response mode.
        PspMaterialLightingResponse LightingResponse;
    };
}
```

Create `src/platform/psp/rendering/PspRuntimeMaterial.cpp` with:

```cpp
#include "platform/psp/rendering/PspRuntimeMaterial.hpp"

#include <cstring>
#include <stdexcept>

#include "MaterialConstantBufferAsset.hpp"

namespace helengine::psp::rendering {
    namespace {
        /// Constant-buffer name used for the authored base color payload.
        constexpr const char* BaseColorBufferName = "BaseColorBuffer";

        /// Constant-buffer name used for the PSP lighting configuration payload.
        constexpr const char* LightingConfigBufferName = "LightingConfigBuffer";
    }

    /// Creates one PSP runtime material with lit-directional defaults.
    PspRuntimeMaterial::PspRuntimeMaterial()
        : BaseColor(1.0f, 1.0f, 1.0f, 1.0f),
          ReceivesLighting(true),
          LightingResponse(PspMaterialLightingResponse::LitDirectional) {
    }

    /// Gets the authored base color used by the PSP renderer.
    const float4& PspRuntimeMaterial::GetBaseColor() const {
        return BaseColor;
    }

    /// Gets whether the material receives scene lighting.
    bool PspRuntimeMaterial::GetReceivesLighting() const {
        return ReceivesLighting;
    }

    /// Gets the PSP lighting-response mode.
    PspMaterialLightingResponse PspRuntimeMaterial::GetLightingResponse() const {
        return LightingResponse;
    }

    /// Loads PSP material state from one cooked material asset.
    void PspRuntimeMaterial::LoadFromCooked(MaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw std::invalid_argument("PSP cooked material data is required.");
        }

        this->set_Id(materialAsset->get_Id());
        if (materialAsset->ConstantBuffers == nullptr) {
            return;
        }

        for (int32_t index = 0; index < materialAsset->ConstantBuffers->Length; index++) {
            MaterialConstantBufferAsset* constantBuffer = (*materialAsset->ConstantBuffers)[index];
            if (constantBuffer == nullptr || constantBuffer->Data == nullptr) {
                continue;
            }

            if (constantBuffer->get_Name() == BaseColorBufferName && constantBuffer->Data->Length >= 16) {
                float components[4] {};
                std::memcpy(components, constantBuffer->Data->Data, sizeof(components));
                BaseColor = float4(components[0], components[1], components[2], components[3]);
            } else if (constantBuffer->get_Name() == LightingConfigBufferName && constantBuffer->Data->Length >= 8) {
                float configuration[2] {};
                std::memcpy(configuration, constantBuffer->Data->Data, sizeof(configuration));
                ReceivesLighting = configuration[0] >= 0.5f;
                LightingResponse = configuration[1] < 0.5f
                    ? PspMaterialLightingResponse::Unlit
                    : PspMaterialLightingResponse::LitDirectional;
            }
        }
    }
}
```

- [ ] **Step 5: Teach the PSP renderer header and build system about the new lighting types**

In `src/platform/psp/rendering/PspRenderManager3D.hpp`, add these includes:

```cpp
#include "platform/psp/rendering/PspLightingSettings.hpp"
#include "platform/psp/rendering/PspSceneLightingSnapshot.hpp"
```

Then add these private declarations inside `PspRenderManager3D`:

```cpp
        /// Resolves the active scene lighting for the current render pass.
        void ResolveSceneLighting();

        /// Stores the renderer-owned PSP lighting settings.
        PspLightingSettings LightingSettings;

        /// Stores the active scene-light snapshot for the current render pass.
        PspSceneLightingSnapshot CurrentLighting;
```

In `CMakeLists.txt`, append the new runtime-lighting sources to `HELENGINE_PSP_SOURCES`:

```cmake
    src/platform/psp/rendering/PspRuntimeMaterial.cpp
```

The header-only files do not need their own `HELENGINE_PSP_SOURCES` entries.

- [ ] **Step 6: Build the PSP player to verify the new lighting scaffolding compiles**

Run: `rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v C:/tmp/psp-generated-core-cube-normalized:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON`

Expected: PASS with `build/EBOOT.PBP` rebuilt and no missing-source or missing-include errors.

- [ ] **Step 7: Commit the PSP runtime lighting types**

```bash
git add CMakeLists.txt src/platform/psp/rendering/PspLightingPipeline.hpp src/platform/psp/rendering/PspMaterialLightingResponse.hpp src/platform/psp/rendering/PspLightingSettings.hpp src/platform/psp/rendering/PspSceneLightingSnapshot.hpp src/platform/psp/rendering/PspRuntimeMaterial.hpp src/platform/psp/rendering/PspRuntimeMaterial.cpp src/platform/psp/rendering/PspRenderManager3D.hpp
git commit -m "Add PSP runtime lighting types"
```

### Task 3: Implement CPU Vertex Lambert Lighting In The PSP Renderer

**Files:**
- Modify: `src/platform/psp/rendering/PspRenderManager3D.cpp`
- Modify: `src/platform/psp/rendering/PspRenderManager3D.hpp`

- [ ] **Step 1: Capture the failing runtime behavior before changing renderer logic**

Launch `C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe` with the current `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP`.

Expected: the cube or directional-light test geometry is still visible but flat white, proving the renderer is submitting geometry without applying scene lighting.

- [ ] **Step 2: Extend the PSP mesh and GU vertex records to preserve normals and per-vertex colors**

In `src/platform/psp/rendering/PspRenderManager3D.cpp`, replace the current internal structs with:

```cpp
        /// Stores one raw PSP mesh payload keyed by the generated runtime-model instance.
        struct PspMeshRecord {
            std::vector<float3> Positions;
            std::vector<float3> Normals;
            std::vector<std::uint32_t> Indices;
        };

        /// Stores one GU vertex with per-vertex color and position.
        struct PspLitVertex {
            std::uint32_t Color;
            float X;
            float Y;
            float Z;
        };
```

Then update `BuildModelFromRaw(...)` so it copies normals in parallel with positions:

```cpp
            if (data->Normals != nullptr && data->Normals->Length > 0) {
                record.Normals.reserve(static_cast<std::size_t>(data->Normals->Length));
                for (int32_t index = 0; index < data->Normals->Length; index++) {
                    record.Normals.push_back((*data->Normals)[index]);
                }
            }
```

- [ ] **Step 3: Switch the renderer to PSP runtime materials instead of a flat-color map**

In `src/platform/psp/rendering/PspRenderManager3D.cpp`, add these includes:

```cpp
#include "DirectionalLightComponent.hpp"
#include "platform/psp/rendering/PspRuntimeMaterial.hpp"
```

Delete the `PspMaterialRecord` struct and the `MaterialRecords` map entirely.

Replace `BuildMaterialFromRaw(...)` with:

```cpp
    RuntimeMaterial* PspRenderManager3D::BuildMaterialFromRaw(MaterialAsset* materialAsset, ShaderAsset* shaderAsset) {
        PspRuntimeMaterial* runtimeMaterial = new PspRuntimeMaterial();
        if (materialAsset != nullptr) {
            runtimeMaterial->LoadFromCooked(materialAsset);
        } else if (shaderAsset != nullptr) {
            runtimeMaterial->set_Id(shaderAsset->get_Id());
        }

        return runtimeMaterial;
    }
```

- [ ] **Step 4: Resolve the first active directional light from the live scene**

In `src/platform/psp/rendering/PspRenderManager3D.cpp`, add this method implementation:

```cpp
    void PspRenderManager3D::ResolveSceneLighting() {
        CurrentLighting = PspSceneLightingSnapshot();

        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr || core->get_ObjectManager()->get_Entities() == nullptr) {
            return;
        }

        List<Entity*>* entities = core->get_ObjectManager()->get_Entities();
        for (int32_t entityIndex = 0; entityIndex < entities->Count(); entityIndex++) {
            Entity* entity = (*entities)[entityIndex];
            if (entity == nullptr || entity->get_Components() == nullptr || !entity->get_IsHierarchyEnabled()) {
                continue;
            }

            List<Component*>* components = entity->get_Components();
            for (int32_t componentIndex = 0; componentIndex < components->Count(); componentIndex++) {
                DirectionalLightComponent* directionalLight = dynamic_cast<DirectionalLightComponent*>((*components)[componentIndex]);
                if (directionalLight == nullptr || directionalLight->get_Parent() == nullptr) {
                    continue;
                }

                CurrentLighting.HasDirectionalLight = true;
                CurrentLighting.DirectionalLightDirection = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), directionalLight->get_Parent()->get_Orientation());
                CurrentLighting.DirectionalLightColor = directionalLight->get_Color();
                CurrentLighting.DirectionalLightIntensity = directionalLight->get_Intensity();
                return;
            }
        }
    }
```

Call `ResolveSceneLighting();` at the start of `RenderCamera(ICamera* camera)` immediately before render-queue traversal.

- [ ] **Step 5: Compute CPU vertex Lambert lighting and submit per-vertex color**

In `src/platform/psp/rendering/PspRenderManager3D.cpp`, add these helper functions inside the anonymous namespace:

```cpp
        float3 RotateNormal(const float3& normal, Entity* entity) {
            if (entity == nullptr) {
                return normal;
            }

            return float4::RotateVector(normal, entity->get_Orientation());
        }

        float4 ClampColor(const float4& color) {
            return float4(
                std::min(std::max(color.X, 0.0f), 1.0f),
                std::min(std::max(color.Y, 0.0f), 1.0f),
                std::min(std::max(color.Z, 0.0f), 1.0f),
                std::min(std::max(color.W, 0.0f), 1.0f));
        }
```

Then replace the material and vertex portion of `Visit(IDrawable3D* drawable)` with:

```cpp
        PspRuntimeMaterial* runtimeMaterial = dynamic_cast<PspRuntimeMaterial*>(drawable->get_Material());
        if (runtimeMaterial == nullptr) {
            return;
        }

        if (LightingSettings.Pipeline == PspLightingPipeline::FixedFunctionLambert) {
            throw std::runtime_error("PspLightingPipeline::FixedFunctionLambert is not implemented yet.");
        }

        const float4 baseColor = runtimeMaterial->GetBaseColor();
        const bool useLighting = runtimeMaterial->GetReceivesLighting()
            && runtimeMaterial->GetLightingResponse() == PspMaterialLightingResponse::LitDirectional;

        int32_t vertexCount = static_cast<int32_t>(record.Indices.empty() ? record.Positions.size() : record.Indices.size());
        if (vertexCount < 3) {
            return;
        }

        CurrentFrameSubmittedVertices += vertexCount;

        PspLitVertex* vertices = static_cast<PspLitVertex*>(sceGuGetMemory(sizeof(PspLitVertex) * static_cast<std::size_t>(vertexCount)));
        for (int32_t index = 0; index < vertexCount; index++) {
            const std::uint32_t sourceIndex = record.Indices.empty()
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

- [ ] **Step 6: Rebuild the PSP player and reinstall the directional-light payload**

Run: `rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v C:/tmp/psp-generated-core-cube-normalized:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON`

Then copy the rebuilt artifacts into:

- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP`
- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\cooked\scenes\rendering\cube_test.hasset`
- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\cooked\engine\models\cube.hasset`
- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\cooked\engine\materials\standard.hasset`
- `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\cooked\shaders\ForwardStandardShader.dx11.hasset`

Expected: PASS with the rebuilt PSP player installed into the PPSSPP memstick tree.

- [ ] **Step 7: Launch PPSSPP and verify that directional lighting replaces the flat-white output**

Launch: `C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe`

Expected:

- startup still reaches `RuntimeMainLoop`
- the test geometry is no longer flat white
- rotating or repositioned geometry shows visibly different lit and unlit faces
- if the authored material is switched to `lighting-response = unlit`, the mesh returns to authored base color without scene-light contribution

- [ ] **Step 8: Commit the CPU directional-light renderer**

```bash
git add src/platform/psp/rendering/PspRenderManager3D.cpp
git commit -m "Add PSP CPU directional lighting"
```

### Task 4: Update PSP Milestone Documentation And Re-Verify End To End

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update the PSP README to reflect the new lighting milestone**

Replace the milestone and verification portions of `README.md` with:

````md
## Current milestone

- Docker-only build using the official PSPDEV SDK image
- Standard PSP homebrew packaging ending in `EBOOT.PBP`
- Generated Helengine runtime sources bundled into the PSP executable
- Runtime startup reaches the real startup scene and main loop
- PSP 3D rendering supports scene-driven directional lighting with CPU vertex Lambert shading
- Ambient lighting currently defaults to `0.25` from PSP renderer settings

## Rendering status

The PSP renderer currently supports:

- live scene camera traversal
- mesh submission from cooked runtime scene data
- base-color materials
- directional-light ambient plus diffuse shading
- unlit material bypass

The PSP renderer does not yet support:

- point lights
- spot lights
- specular
- emissive
- shadows
- fixed-function GU lighting backend
````

- [ ] **Step 2: Run the PSP builder tests after the final lighting changes**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS.

- [ ] **Step 3: Rebuild the PSP player one final time**

Run: `rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v C:/tmp/psp-generated-core-cube-normalized:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON`

Expected: PASS with `build/EBOOT.PBP` rebuilt successfully.

- [ ] **Step 4: Re-run the final PPSSPP verification with the directional-light test scene**

Launch `C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe` with the rebuilt memstick content installed.

Expected:

1. the scene boots without regressing startup-scene load
2. geometry renders with non-flat directional lighting
3. changing light direction in the test scene changes the lit face as expected
4. the same cooked assets remain valid for future fixed-function backend work

- [ ] **Step 5: Commit the documentation and final verification pass**

```bash
git add README.md
git commit -m "Document PSP directional lighting milestone"
```
