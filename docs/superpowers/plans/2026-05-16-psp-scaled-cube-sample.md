# PSP Scaled Cube Sample Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add one permanent rendering sample scene with a single `5x20x10` cube and an orbit camera, place it immediately after `cube_test` in the Demo Disc menu, and include it in normal PSP full exports.

**Architecture:** The sample stays generator-owned like the existing rendering showcase scenes. One new city scene factory will create the authored scene, `RenderingSceneGenerator` will register and write it, `DemoDiscSceneCatalog` will insert it as the second rendering item, and the city build config will include it in the Windows and PSP full scene lists so menu selection never points at a missing cooked scene.

**Tech Stack:** City generated authored scenes, `rendering.tools` scene factories, Demo Disc menu definitions, HelEngine headless editor build/export flow, PPSSPP verification.

---

## File Map

- Create: `C:\dev\helprojs\city\assets\codebase\rendering.tools\ScaledCubeSceneFactory.cs`
  - Owns the new generated authored rendering sample scene.
- Modify: `C:\dev\helprojs\city\assets\codebase\rendering.tools\RenderingSceneGenerator.cs`
  - Adds the new scene id constant, factory field, construction, and write call.
- Modify: `C:\dev\helprojs\city\assets\codebase\menu\DemoDiscSceneCatalog.cs`
  - Inserts the new selectable scene item immediately after `cube_test`.
- Modify: `C:\dev\helprojs\city\user_settings\build_config.json`
  - Adds the new scene to the Windows and PSP selected scene lists and scene order lists.
- Generated output to verify:
  - `C:\dev\helprojs\city\assets\Scenes\rendering\scaled_cube.helen`
  - `C:\dev\helprojs\output\psp-demo-disc-full\PSP\GAME\HELENGINE\cooked\scenes\rendering\scaled_cube.hasset`

### Task 1: Add the generated scaled-cube scene factory

**Files:**
- Create: `C:\dev\helprojs\city\assets\codebase\rendering.tools\ScaledCubeSceneFactory.cs`
- Modify: `C:\dev\helprojs\city\assets\codebase\rendering.tools\RenderingSceneGenerator.cs`

- [ ] **Step 1: Create the new scene-factory file**

Create `ScaledCubeSceneFactory.cs` with the same structural pattern used by `CubeTestSceneFactory.cs`, but with a static cube and an orbit-only camera:

```csharp
using city.menu;
using gameplay.rendering;
using helengine.editor;

namespace city.rendering.tools {
    /// <summary>
    /// Builds the canonical live-authored scene definition for the scaled-cube rendering test.
    /// </summary>
    public sealed class ScaledCubeSceneFactory {
        /// <summary>
        /// Stable scene id used by the generated scaled-cube asset.
        /// </summary>
        public const string SceneId = RenderingSceneGenerator.ScaledCubeSceneId;

        /// <summary>
        /// Initializes one scaled-cube scene factory.
        /// </summary>
        public ScaledCubeSceneFactory() { }

        /// <summary>
        /// Creates the canonical scaled-cube live scene definition.
        /// </summary>
        /// <param name="cubeModel">Generated cube runtime model assigned to the authored mesh.</param>
        /// <param name="standardMaterial">Generated standard runtime material assigned to the authored mesh.</param>
        /// <returns>Live-authored scaled-cube scene definition.</returns>
        public GeneratedAuthoringSceneDefinition CreateSceneDefinition(RuntimeModel cubeModel, RuntimeMaterial standardMaterial) {
            if (cubeModel == null) {
                throw new ArgumentNullException(nameof(cubeModel));
            } else if (standardMaterial == null) {
                throw new ArgumentNullException(nameof(standardMaterial));
            }

            return new GeneratedAuthoringSceneDefinition {
                SceneId = SceneId,
                SceneSettings = new SceneSettingsAsset(),
                RootEntities = new[] {
                    CreateCameraEntity(),
                    CreateDirectionalLightEntity(),
                    CreateCubeEntity(cubeModel, standardMaterial)
                }
            };
        }

        Entity CreateCameraEntity() {
            float4 orientation;
            float4.CreateFromYawPitchRoll(0f, -0.28f, 0f, out orientation);

            Entity entity = Core.Instance.EntityFactory.Create("ScaledCubeCamera");
            entity.LocalPosition = new float3(0f, 18f, 48f);
            entity.LocalScale = float3.One;
            entity.LocalOrientation = orientation;
            entity.AddComponent(new CameraComponent {
                CameraDrawOrder = 0,
                LayerMask = EditorLayerMasks.SceneObjects,
                Viewport = new float4(0f, 0f, 1f, 1f),
                NearPlaneDistance = 0.1f,
                FarPlaneDistance = 128f,
                ClearSettings = new CameraClearSettings(
                    true,
                    new float4(100f / 255f, 149f / 255f, 237f / 255f, 1f),
                    true,
                    1f,
                    false,
                    0),
                RenderSettings = new CameraRenderSettings {
                    DepthPrepassMode = DepthPrepassMode.Auto,
                    ShadowDistance = 48f,
                    PostProcessTier = PostProcessTier.Disabled
                }
            });
            entity.AddComponent(new DirectionalShadowCameraOrbitComponent {
                OrbitCenter = new float3(0f, 10f, 0f),
                OrbitRadius = 48f,
                OrbitHeight = 18f,
                BaseAngleRadians = 0f,
                AngularSpeedRadians = 0.07f,
                LookDownPitchRadians = -0.28f
            });
            entity.AddComponent(new FPSComponent {
                Font = ResolveRequiredEditorFont()
            });
            entity.AddComponent(new DemoDiscReturnToMenuComponent());
            return entity;
        }

        Entity CreateDirectionalLightEntity() {
            float4 orientation;
            float4.CreateFromYawPitchRoll(-0.65f, -0.85f, 0f, out orientation);

            Entity entity = Core.Instance.EntityFactory.Create("ScaledCubeSun");
            entity.LocalPosition = new float3(0f, 8f, 0f);
            entity.LocalScale = float3.One;
            entity.LocalOrientation = orientation;
            entity.AddComponent(new DirectionalLightComponent {
                Color = new float4(1f, 1f, 1f, 1f),
                Intensity = 1f,
                ShadowsEnabled = false,
                ShadowMapMode = ShadowMapMode.Forced,
                ShadowStrength = 1f,
                ShadowDistance = 48f
            });
            return entity;
        }

        Entity CreateCubeEntity(RuntimeModel cubeModel, RuntimeMaterial standardMaterial) {
            Entity entity = Core.Instance.EntityFactory.Create("ScaledCube");
            entity.LocalPosition = new float3(0f, 10f, 0f);
            entity.LocalScale = new float3(5f, 20f, 10f);
            entity.LocalOrientation = float4.Identity;
            entity.AddComponent(new MeshComponent {
                Model = cubeModel,
                Material = standardMaterial,
                RenderOrder3D = 0
            });
            return entity;
        }

        FontAsset ResolveRequiredEditorFont() {
            if (Core.Instance is not EditorCore editorCore || editorCore.DefaultFontAssetForEditor == null) {
                throw new InvalidOperationException("A default editor font must be loaded before the scaled-cube scene can be generated.");
            }

            return editorCore.DefaultFontAssetForEditor;
        }
    }
}
```

- [ ] **Step 2: Register the new scene in the rendering scene generator**

Modify `RenderingSceneGenerator.cs` to add the scene id constant, factory field, constructor initialization, and generation/write call:

```csharp
/// <summary>
/// Stable scene id used by the scaled-cube showcase.
/// </summary>
public const string ScaledCubeSceneId = "scenes/rendering/scaled_cube.helen";

/// <summary>
/// Factory used to author the scaled-cube scene.
/// </summary>
readonly ScaledCubeSceneFactory ScaledCubeFactory;

public RenderingSceneGenerator() {
    SceneWriteService = new GeneratedSceneWriteService();
    AuthoringSceneWriteService = new GeneratedAuthoringSceneWriteService();
    DirectionalShadowPlazaFactory = new DirectionalShadowPlazaSceneFactory();
    SpotlightStreetSliceFactory = new SpotlightStreetSliceSceneFactory();
    CubeTestFactory = new CubeTestSceneFactory();
    ScaledCubeFactory = new ScaledCubeSceneFactory();
    ColoredCubeGridFactory = new ColoredCubeGridSceneFactory();
    TexturedCubeGridFactory = new TexturedCubeGridSceneFactory();
    AxisTestFactory = new AxisTestSceneFactory();
    AxisTest2Factory = new AxisTest2SceneFactory();
}
```

Add the new scene definition in `Generate(...)`:

```csharp
GeneratedAuthoringSceneDefinition cubeTestSceneDefinition = CubeTestFactory.CreateSceneDefinition(assets.GeneratedCubeModel, assets.GeneratedStandardMaterial);
GeneratedAuthoringSceneDefinition scaledCubeSceneDefinition = ScaledCubeFactory.CreateSceneDefinition(assets.GeneratedCubeModel, assets.GeneratedStandardMaterial);
GeneratedAuthoringSceneDefinition coloredCubeGridSceneDefinition;
```

Write it immediately after `cube_test`:

```csharp
AuthoringSceneWriteService.WriteScene(projectRootPath, cubeTestSceneDefinition);
AuthoringSceneWriteService.WriteScene(projectRootPath, scaledCubeSceneDefinition);
AuthoringSceneWriteService.WriteScene(projectRootPath, coloredCubeGridSceneDefinition);
```

- [ ] **Step 3: Regenerate the rendering scenes**

Run:

```powershell
& 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.exe' --project 'C:\dev\helprojs\city' --editor-command menu.generate-rendering-scenes
```

Expected:
- command exits successfully
- `C:\dev\helprojs\city\assets\Scenes\rendering\scaled_cube.helen` exists

- [ ] **Step 4: Verify the authored scene file exists**

Run:

```powershell
Get-Item 'C:\dev\helprojs\city\assets\Scenes\rendering\scaled_cube.helen' | Select-Object FullName,Length,LastWriteTime
```

Expected:
- one scene file exists at that exact path

- [ ] **Step 5: Commit**

```bash
git add C:/dev/helprojs/city/assets/codebase/rendering.tools/ScaledCubeSceneFactory.cs C:/dev/helprojs/city/assets/codebase/rendering.tools/RenderingSceneGenerator.cs C:/dev/helprojs/city/assets/Scenes/rendering/scaled_cube.helen
git commit -m "feat: add generated scaled cube sample scene"
```

### Task 2: Insert the new sample into the Demo Disc scene list

**Files:**
- Modify: `C:\dev\helprojs\city\assets\codebase\menu\DemoDiscSceneCatalog.cs`
- Modify: `C:\dev\helprojs\city\assets\Scenes\DemoDiscMainMenu.helen` (regenerated output)

- [ ] **Step 1: Insert the new menu item immediately after cube_test**

Modify `DemoDiscSceneCatalog.cs` so the new item is second:

```csharp
return new[] {
    new MenuItemDefinition("scene-cube-test", "Cube Test", "Minimal one-cube rendering validation scene.", true, new MenuActionDefinition(MenuActionKind.LoadScene, "cube_test")),
    new MenuItemDefinition("scene-scaled-cube", "Scaled Cube", "Single large non-uniformly scaled cube with an orbit camera.", true, new MenuActionDefinition(MenuActionKind.LoadScene, "scaled_cube")),
    new MenuItemDefinition("scene-colored-cube-grid", "Colored Cubes", "Sixteen rotating cubes with distinct lit material colors.", true, new MenuActionDefinition(MenuActionKind.LoadScene, "colored_cube_grid")),
    new MenuItemDefinition("scene-textured-cube-grid", "Textured Cubes", "Sixteen rotating cubes with distinct lit texture materials.", true, new MenuActionDefinition(MenuActionKind.LoadScene, "textured_cube_grid")),
    new MenuItemDefinition("scene-axis-test", "Axis 1", "Three-axis rotation validation scene with a directional-light arrow.", true, new MenuActionDefinition(MenuActionKind.LoadScene, "axis_test")),
    new MenuItemDefinition("scene-axis-test-2", "Axis 2", "Mirrored axis showcase that validates the right-side directional layout.", true, new MenuActionDefinition(MenuActionKind.LoadScene, "axis_test2")),
    new MenuItemDefinition("scene-directional-shadow-plaza", "Directional Shadow Plaza", "Lighting showcase with an orbiting camera and decorative plaza geometry.", true, new MenuActionDefinition(MenuActionKind.LoadScene, "directional_shadow_plaza")),
    new MenuItemDefinition("scene-spotlight-street-slice", "Spotlight Street Slice", "Night street showcase with lamppost lighting and the racer hero model.", true, new MenuActionDefinition(MenuActionKind.LoadScene, "spotlight_street_slice")),
    new MenuItemDefinition("scene-back", "Back", "Returns to the main menu.", true, new MenuActionDefinition(MenuActionKind.Back, string.Empty))
};
```

- [ ] **Step 2: Regenerate the Demo Disc main menu scene**

Run:

```powershell
& 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.exe' --project 'C:\dev\helprojs\city' --editor-command menu.regenerate-demo-disc-main-menu
```

Expected:
- command exits successfully
- `C:\dev\helprojs\city\assets\Scenes\DemoDiscMainMenu.helen` is updated

- [ ] **Step 3: Sanity-check the authored menu scene regenerated**

Run:

```powershell
Get-Item 'C:\dev\helprojs\city\assets\Scenes\DemoDiscMainMenu.helen' | Select-Object FullName,Length,LastWriteTime
```

Expected:
- file exists
- timestamp reflects the regeneration step

- [ ] **Step 4: Commit**

```bash
git add C:/dev/helprojs/city/assets/codebase/menu/DemoDiscSceneCatalog.cs C:/dev/helprojs/city/assets/Scenes/DemoDiscMainMenu.helen
git commit -m "feat: add scaled cube sample to demo disc menu"
```

### Task 3: Include the scene in full Windows and PSP build scene lists

**Files:**
- Modify: `C:\dev\helprojs\city\user_settings\build_config.json`

- [ ] **Step 1: Add the new scene id to the Windows scene list and order**

Modify the Windows platform block in `build_config.json`:

```json
"selectedSceneIds": [
  "DemoDiscMainMenu",
  "cube_test",
  "scaled_cube",
  "colored_cube_grid",
  "textured_cube_grid",
  "axis_test",
  "axis_test2",
  "directional_shadow_plaza",
  "spotlight_street_slice"
],
"sceneOrders": [
  { "sceneId": "spotlight_street_slice", "orderNumber": 1 },
  { "sceneId": "DemoDiscMainMenu", "orderNumber": 2 },
  { "sceneId": "cube_test", "orderNumber": 3 },
  { "sceneId": "scaled_cube", "orderNumber": 4 },
  { "sceneId": "colored_cube_grid", "orderNumber": 5 },
  { "sceneId": "textured_cube_grid", "orderNumber": 6 },
  { "sceneId": "axis_test", "orderNumber": 7 },
  { "sceneId": "axis_test2", "orderNumber": 8 },
  { "sceneId": "directional_shadow_plaza", "orderNumber": 9 }
]
```

- [ ] **Step 2: Add the new scene id to the PSP scene list and order**

Modify the PSP platform block in `build_config.json`:

```json
"selectedSceneIds": [
  "DemoDiscMainMenu",
  "cube_test",
  "scaled_cube",
  "colored_cube_grid",
  "textured_cube_grid",
  "axis_test",
  "axis_test2",
  "directional_shadow_plaza",
  "spotlight_street_slice"
],
"sceneOrders": [
  { "sceneId": "DemoDiscMainMenu", "orderNumber": 1 },
  { "sceneId": "cube_test", "orderNumber": 2 },
  { "sceneId": "scaled_cube", "orderNumber": 3 },
  { "sceneId": "colored_cube_grid", "orderNumber": 4 },
  { "sceneId": "textured_cube_grid", "orderNumber": 5 },
  { "sceneId": "axis_test", "orderNumber": 6 },
  { "sceneId": "axis_test2", "orderNumber": 7 },
  { "sceneId": "directional_shadow_plaza", "orderNumber": 8 },
  { "sceneId": "spotlight_street_slice", "orderNumber": 9 }
]
```

- [ ] **Step 3: Commit**

```bash
git add C:/dev/helprojs/city/user_settings/build_config.json
git commit -m "chore: include scaled cube sample in full build scene lists"
```

### Task 4: Build and verify the PSP export end to end

**Files:**
- Verify: `C:\dev\helprojs\output\psp-demo-disc-full\PSP\GAME\HELENGINE\EBOOT.PBP`
- Verify: `C:\dev\helprojs\output\psp-demo-disc-full\PSP\GAME\HELENGINE\cooked\scenes\rendering\scaled_cube.hasset`
- Verify: `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP`

- [ ] **Step 1: Build the full PSP export**

Run:

```powershell
& 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.exe' --project 'C:\dev\helprojs\city' --build psp --output 'C:\dev\helprojs\output\psp-demo-disc-full'
```

Expected:
- build completes successfully
- `C:\dev\helprojs\output\psp-demo-disc-full\PSP\GAME\HELENGINE\EBOOT.PBP` exists

- [ ] **Step 2: Verify the cooked scaled-cube scene exists in the PSP export**

Run:

```powershell
Get-Item 'C:\dev\helprojs\output\psp-demo-disc-full\PSP\GAME\HELENGINE\cooked\scenes\rendering\scaled_cube.hasset' | Select-Object FullName,Length,LastWriteTime
```

Expected:
- one cooked scene asset exists at that exact path

- [ ] **Step 3: Restage the PPSSPP memstick build**

Run:

```powershell
Copy-Item 'C:\dev\helprojs\output\psp-demo-disc-full\PSP\GAME\HELENGINE\*' 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE' -Recurse -Force
```

Expected:
- memstick game folder is updated with the latest export

- [ ] **Step 4: Launch PPSSPP on the staged build**

Run:

```powershell
Start-Process -FilePath 'C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe' -ArgumentList 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP'
```

Expected:
- PPSSPP opens the demo-disc build

- [ ] **Step 5: Manual runtime verification**

Verify manually in PPSSPP:

```text
1. Open the Demo Disc main menu.
2. Confirm "Scaled Cube" appears immediately after "Cube Test".
3. Launch the sample.
4. Confirm the scene shows one static large cube.
5. Confirm the orbit camera keeps the cube framed and looking readable.
```

Expected:
- item ordering is correct
- scene loads
- cube scale looks like `5x20x10`
- only the camera moves

- [ ] **Step 6: Commit**

```bash
git add C:/dev/helprojs/city/assets/codebase/rendering.tools/ScaledCubeSceneFactory.cs C:/dev/helprojs/city/assets/codebase/rendering.tools/RenderingSceneGenerator.cs C:/dev/helprojs/city/assets/codebase/menu/DemoDiscSceneCatalog.cs C:/dev/helprojs/city/user_settings/build_config.json C:/dev/helprojs/city/assets/Scenes/rendering/scaled_cube.helen C:/dev/helprojs/city/assets/Scenes/DemoDiscMainMenu.helen
git commit -m "feat: add scaled cube rendering sample"
```

## Self-Review

### Spec coverage

- Permanent generated scene: covered by Task 1.
- Static `5x20x10` cube with orbit camera: covered by Task 1 scene-factory implementation.
- Demo Disc selectable item: covered by Task 2.
- Inserted immediately after `cube_test`: covered by Task 2 and Task 3 ordering updates.
- PSP full export inclusion: covered by Task 3 and Task 4.

### Placeholder scan

- No `TBD`, `TODO`, or “similar to” placeholders remain.
- Every modified file path is explicit.
- Every verification command is explicit.

### Type consistency

- Scene id is consistently `scaled_cube`.
- Generated authored file path is consistently `scenes/rendering/scaled_cube.helen`.
- Menu action target is consistently `scaled_cube`.
- Orbit behavior is camera-only; no rotation component is added to the cube entity.
