# PSP Demo Disc Menu 2D Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `DemoDiscMainMenu` render and behave interactively on PSP by adding a reusable 2D renderer slice for menu surfaces, sprites, and bitmap text.

**Architecture:** Keep the existing engine menu runtime unchanged and raise the PSP backend to match the current 2D drawable contracts. Add PSP-side solid/sprite/text rendering in `PspRenderManager2D`, isolate font-specific runtime caching in a dedicated helper, and use builder regression coverage to prove the menu payload contains the scene and font assets the runtime needs.

**Tech Stack:** C++, PSP GU/GUM, existing generated-core runtime, .NET builder tests, PPSSPP for runtime verification

---

## File Structure

- Modify: `src/platform/psp/rendering/PspRenderManager2D.hpp`
  - Extend the PSP 2D renderer surface from no-op placeholders into real rounded-rect, sprite, and text drawing entry points.
- Modify: `src/platform/psp/rendering/PspRenderManager2D.cpp`
  - Implement the PSP 2D draw path, color/texture state management, glyph quad submission, and ordering-safe primitive helpers.
- Create: `src/platform/psp/rendering/PspFontCache.hpp`
  - Declare PSP font runtime caching and glyph lookup helpers separate from the main renderer.
- Create: `src/platform/psp/rendering/PspFontCache.cpp`
  - Build/reuse font atlas textures and expose glyph metrics needed by `DrawText(...)`.
- Modify: `CMakeLists.txt`
  - Add the new PSP font cache translation unit to the PSP build.
- Create: `builder.tests/CityDemoDiscMainMenuSceneTests.cs`
  - Add regression coverage proving the PSP export packages the demo menu scene and both menu fonts.
- Modify: `README.md`
  - Document the PSP menu milestone and the manual PPSSPP verification flow once the feature is working.

## Task 1: Add Demo Disc Menu PSP Builder Regression Coverage

**Files:**
- Create: `builder.tests/CityDemoDiscMainMenuSceneTests.cs`
- Test: `builder.tests/helengine.psp.builder.tests.csproj`

- [ ] **Step 1: Write the failing test**

```csharp
using System.IO;
using Xunit;

namespace helengine.psp.builder.tests {
    public sealed class CityDemoDiscMainMenuSceneTests {
        [Fact]
        public void DemoDiscMainMenu_export_includes_scene_and_fonts() {
            string packageRoot = Path.Combine(
                @"C:\tmp\city-psp-demo-disc-menu-build",
                "PSP",
                "GAME",
                "HELENGINE");

            string scenePath = Path.Combine(packageRoot, "cooked", "scenes", "DemoDiscMainMenu.hasset");
            string titleFontPath = Path.Combine(packageRoot, "cooked", "fonts", "DemoDiscTitle.ttf.hasset");
            string bodyFontPath = Path.Combine(packageRoot, "cooked", "fonts", "DemoDiscBody.ttf.hasset");

            Assert.True(File.Exists(scenePath), $"Expected cooked menu scene at {scenePath}.");
            Assert.True(File.Exists(titleFontPath), $"Expected cooked title font at {titleFontPath}.");
            Assert.True(File.Exists(bodyFontPath), $"Expected cooked body font at {bodyFontPath}.");
        }
    }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~CityDemoDiscMainMenuSceneTests -v minimal
```

Expected: `FAIL` because the test file does not exist yet or because the demo menu export has not been produced in the expected location.

- [ ] **Step 3: Write the minimal test file**

```csharp
using System.IO;
using Xunit;

namespace helengine.psp.builder.tests {
    public sealed class CityDemoDiscMainMenuSceneTests {
        [Fact]
        public void DemoDiscMainMenu_export_includes_scene_and_fonts() {
            string packageRoot = Path.Combine(
                @"C:\tmp\city-psp-demo-disc-menu-build",
                "PSP",
                "GAME",
                "HELENGINE");

            string scenePath = Path.Combine(packageRoot, "cooked", "scenes", "DemoDiscMainMenu.hasset");
            string titleFontPath = Path.Combine(packageRoot, "cooked", "fonts", "DemoDiscTitle.ttf.hasset");
            string bodyFontPath = Path.Combine(packageRoot, "cooked", "fonts", "DemoDiscBody.ttf.hasset");

            Assert.True(File.Exists(scenePath), $"Expected cooked menu scene at {scenePath}.");
            Assert.True(File.Exists(titleFontPath), $"Expected cooked title font at {titleFontPath}.");
            Assert.True(File.Exists(bodyFontPath), $"Expected cooked body font at {bodyFontPath}.");
        }
    }
}
```

- [ ] **Step 4: Run the targeted test after exporting the menu payload**

Run:

```powershell
$env:HELENGINE_PSP_REPOSITORY_ROOT='C:\dev\helworks\helengine-psp'
dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city --build psp --output C:\tmp\city-psp-demo-disc-menu-build
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~CityDemoDiscMainMenuSceneTests -v minimal
```

Expected: `PASS` once the fresh PSP export contains `cooked/scenes/DemoDiscMainMenu.hasset` and the cooked fonts.

- [ ] **Step 5: Commit**

```bash
git add builder.tests/CityDemoDiscMainMenuSceneTests.cs
git commit -m "Add PSP demo menu export regression"
```

## Task 2: Replace PSP 2D No-Ops With Solid And Sprite Rendering

**Files:**
- Modify: `src/platform/psp/rendering/PspRenderManager2D.hpp`
- Modify: `src/platform/psp/rendering/PspRenderManager2D.cpp`
- Test: `builder.tests/CityDemoDiscMainMenuSceneTests.cs`

- [ ] **Step 1: Write the failing runtime expectation in code comments and helper signatures**

Add the renderer surface that the implementation will satisfy:

```cpp
class PspRenderManager2D final : public RenderManager2D {
public:
    RuntimeTexture* BuildTextureFromRaw(TextureAsset* data) override;
    void DrawRoundedRect(IRoundedRectDrawable2D* shape) override;
    void DrawSprite(ISpriteDrawable2D* sprite) override;
    void DrawText(ITextDrawable2D* text) override;

private:
    struct Psp2DVertex {
        float U;
        float V;
        uint32_t Color;
        float X;
        float Y;
        float Z;
    };

    PspTextureCache TextureCache;

    void DrawSolidQuad(const float3& position, const float2& size, uint32_t color);
    void DrawTexturedQuad(RuntimeTexture* texture, const float3& position, const float2& size, const float4& uvRect, uint32_t color);
    uint32_t ConvertColor(const float4& color) const;
};
```

- [ ] **Step 2: Run the menu build and PPSSPP once to capture the current failure**

Run:

```powershell
rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v C:/Users/Helena/AppData/Local/Temp/helengine-platform-build/psp/c57996aeeb1d4295b2a413d760b085b5/generated-core:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

Expected: successful rebuild, followed by a PPSSPP launch where the user still reports that the main menu does not render.

- [ ] **Step 3: Implement solid quads and sprite quads**

In `src/platform/psp/rendering/PspRenderManager2D.cpp`, replace the no-ops with real GU draws:

```cpp
namespace helengine::psp::rendering {
    RuntimeTexture* PspRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        return TextureCache.BuildTextureFromRaw(data);
    }

    void PspRenderManager2D::DrawRoundedRect(IRoundedRectDrawable2D* shape) {
        if (shape == nullptr) {
            return;
        }

        DrawSolidQuad(shape->get_Position(), shape->get_Size(), ConvertColor(shape->get_Color()));
    }

    void PspRenderManager2D::DrawSprite(ISpriteDrawable2D* sprite) {
        if (sprite == nullptr || sprite->get_Texture() == nullptr) {
            return;
        }

        RuntimeTexture* texture = BuildTextureFromRaw(sprite->get_Texture());
        if (texture == nullptr) {
            return;
        }

        DrawTexturedQuad(
            texture,
            sprite->get_Position(),
            sprite->get_Size(),
            sprite->get_SourceRectNormalized(),
            ConvertColor(sprite->get_Color()));
    }

    uint32_t PspRenderManager2D::ConvertColor(const float4& color) const {
        const uint32_t a = static_cast<uint32_t>(color.W * 255.0f) & 0xFF;
        const uint32_t b = static_cast<uint32_t>(color.Z * 255.0f) & 0xFF;
        const uint32_t g = static_cast<uint32_t>(color.Y * 255.0f) & 0xFF;
        const uint32_t r = static_cast<uint32_t>(color.X * 255.0f) & 0xFF;
        return (a << 24) | (b << 16) | (g << 8) | r;
    }
}
```

- [ ] **Step 4: Rebuild and verify that surfaces/sprites start appearing**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~CityDemoDiscMainMenuSceneTests -v minimal
rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v C:/Users/Helena/AppData/Local/Temp/helengine-platform-build/psp/c57996aeeb1d4295b2a413d760b085b5/generated-core:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

Expected: builder regression still passes, PSP rebuild still succeeds, and the next PPSSPP launch should move from “nothing visible” to at least panel/surface output.

- [ ] **Step 5: Commit**

```bash
git add src/platform/psp/rendering/PspRenderManager2D.hpp src/platform/psp/rendering/PspRenderManager2D.cpp
git commit -m "Add PSP 2D solid and sprite rendering"
```

## Task 3: Add PSP Bitmap Font Rendering

**Files:**
- Create: `src/platform/psp/rendering/PspFontCache.hpp`
- Create: `src/platform/psp/rendering/PspFontCache.cpp`
- Modify: `src/platform/psp/rendering/PspRenderManager2D.hpp`
- Modify: `src/platform/psp/rendering/PspRenderManager2D.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing font-cache interface**

Create the dedicated font cache boundary:

```cpp
#pragma once

#include "FontAsset.hpp"
#include "RuntimeTexture.hpp"
#include <unordered_map>

namespace helengine::psp::rendering {
    class PspFontCache final {
    public:
        struct GlyphRuntime {
            float4 UvRect;
            float2 Size;
            float2 Offset;
            float Advance;
        };

        RuntimeTexture* ResolveAtlas(FontAsset* fontAsset);
        bool TryResolveGlyph(FontAsset* fontAsset, int32_t codepoint, GlyphRuntime& glyphRuntime);

    private:
        std::unordered_map<FontAsset*, RuntimeTexture*> AtlasByFont;
    };
}
```

- [ ] **Step 2: Run the PSP rebuild without the implementation to verify the current menu is still missing text**

Run:

```powershell
rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v C:/Users/Helena/AppData/Local/Temp/helengine-platform-build/psp/c57996aeeb1d4295b2a413d760b085b5/generated-core:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

Expected: rebuild succeeds, but the next PPSSPP launch still lacks title/body text because `DrawText(...)` is still not implemented.

- [ ] **Step 3: Implement glyph quad submission**

Add the font cache to the renderer and implement `DrawText(...)`:

```cpp
void PspRenderManager2D::DrawText(ITextDrawable2D* text) {
    if (text == nullptr || text->get_Font() == nullptr) {
        return;
    }

    RuntimeTexture* atlasTexture = FontCache.ResolveAtlas(text->get_Font());
    if (atlasTexture == nullptr) {
        return;
    }

    float3 pen = text->get_Position();
    std::string value = text->get_Text();
    for (char character : value) {
        PspFontCache::GlyphRuntime glyph;
        if (!FontCache.TryResolveGlyph(text->get_Font(), static_cast<int32_t>(character), glyph)) {
            continue;
        }

        DrawTexturedQuad(
            atlasTexture,
            float3(pen.X + glyph.Offset.X, pen.Y + glyph.Offset.Y, pen.Z),
            glyph.Size,
            glyph.UvRect,
            ConvertColor(text->get_Color()));

        pen.X += glyph.Advance;
    }
}
```

Also wire the new implementation into the build:

```cmake
set(HELENGINE_PSP_SOURCES
    src/main.cpp
    src/platform/psp/rendering/PspRenderManager2D.cpp
    src/platform/psp/rendering/PspFontCache.cpp
)
```

- [ ] **Step 4: Rebuild and verify menu text appears**

Run:

```powershell
rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v C:/Users/Helena/AppData/Local/Temp/helengine-platform-build/psp/c57996aeeb1d4295b2a413d760b085b5/generated-core:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

Expected: rebuild succeeds, and the next PPSSPP launch should show readable title/body text in addition to menu surfaces.

- [ ] **Step 5: Commit**

```bash
git add src/platform/psp/rendering/PspFontCache.hpp src/platform/psp/rendering/PspFontCache.cpp src/platform/psp/rendering/PspRenderManager2D.hpp src/platform/psp/rendering/PspRenderManager2D.cpp CMakeLists.txt
git commit -m "Add PSP bitmap font rendering"
```

## Task 4: Verify Full Menu Interaction And Document The Milestone

**Files:**
- Modify: `README.md`
- Test: `builder.tests/helengine.psp.builder.tests.csproj`

- [ ] **Step 1: Add the README milestone note**

Append a short PSP milestone note once the menu works:

```md
## PSP Demo Disc Menu

The PSP runtime now supports the 2D feature slice required by `DemoDiscMainMenu`:

- solid and rounded menu surfaces
- sprite rendering
- bitmap font text
- menu interaction through PSP input

Use the existing PSP export flow, then copy `cooked/*` into PPSSPP memstick before copying the rebuilt `EBOOT.PBP` last.
```

- [ ] **Step 2: Run the full PSP builder test suite**

Run:

```powershell
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal
```

Expected: `PASS` for all PSP builder tests, including the new demo menu regression.

- [ ] **Step 3: Export, install, and verify interaction manually in PPSSPP**

Run:

```powershell
$env:HELENGINE_PSP_REPOSITORY_ROOT='C:\dev\helworks\helengine-psp'
dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city --build psp --output C:\tmp\city-psp-demo-disc-menu-build
rtk dotnet run --project builder/helengine.psp.builder.csproj -- --normalize-generated-core C:\Users\Helena\AppData\Local\Temp\helengine-platform-build\psp\c57996aeeb1d4295b2a413d760b085b5\generated-core
rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v C:/Users/Helena/AppData/Local/Temp/helengine-platform-build/psp/c57996aeeb1d4295b2a413d760b085b5/generated-core:/generated-core -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
Copy-Item C:\tmp\city-psp-demo-disc-menu-build\PSP\GAME\HELENGINE\cooked\* C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\cooked -Recurse -Force
Copy-Item C:\dev\helworks\helengine-psp\build\EBOOT.PBP C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP -Force
Start-Process C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe '\"C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP\"'
```

Expected:

- `DemoDiscMainMenu` renders visibly
- selection/highlight is readable
- PSP input can move selection
- confirm opens subpanels
- confirm can load a scene
- the in-scene back path returns to the menu

After the launch, stop and ask the user exactly what they see before concluding the result.

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "Document PSP demo menu milestone"
```

## Self-Review

- Spec coverage: renderer primitives, text, draw ordering, interaction, and verification are all represented by the tasks above.
- Placeholder scan: no `TODO`, `TBD`, or deferred “write tests later” steps remain.
- Type consistency: the plan uses the existing PSP renderer entry points (`DrawRoundedRect`, `DrawSprite`, `DrawText`) and introduces only one dedicated new helper boundary (`PspFontCache`) to keep font runtime logic isolated.
