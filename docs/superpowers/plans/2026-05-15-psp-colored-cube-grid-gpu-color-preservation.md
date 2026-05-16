# PSP Colored Cube Grid GPU Color Preservation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `colored_cube_grid` render through the PSP GPU directional-lighting path while preserving the distinct authored per-cube base colors.

**Architecture:** Reuse the existing narrow GPU directional path from the `cube_test` milestone and widen confidence in it by validating repeated untextured directional-lit draws with different base colors. Keep startup-scene selection temporary for verification, preserve CPU fallback for unsupported cases, and change only the per-draw GU material state needed to stop color leakage if the scene exposes it.

**Tech Stack:** C++, PSP GU fixed-function rendering, C# builder tests, PowerShell, PPSSPP, Docker PSP toolchain

---

### Task 1: Lock authored per-cube PSP color intent in tests

**Files:**
- Modify: `C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting\builder.tests\CityColoredCubeGridSceneTests.cs`
- Test: `C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting\builder.tests\CityColoredCubeGridSceneTests.cs`

- [ ] **Step 1: Add a failing regression that checks all sixteen PSP material colors, not just `Cube00`**

Add these members near the existing material constants in `builder.tests\CityColoredCubeGridSceneTests.cs`:

```csharp
    /// <summary>
    /// Relative authored material paths for the sixteen colored cube materials.
    /// </summary>
    static readonly string[] CubeMaterialRelativePaths = [
        @"assets\Materials\rendering\colored_cube_grid\Cube00.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube01.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube02.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube03.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube04.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube05.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube06.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube07.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube08.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube09.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube10.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube11.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube12.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube13.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube14.helmat",
        @"assets\Materials\rendering\colored_cube_grid\Cube15.helmat"
    ];

    /// <summary>
    /// Authored PSP base colors expected for the sixteen cube materials.
    /// </summary>
    static readonly string[] ExpectedCubeMaterialColors = [
        "#FF4040FF", "#FF8040FF", "#FFC040FF", "#FFFF40FF",
        "#C0FF40FF", "#80FF40FF", "#40FF40FF", "#40FF80FF",
        "#40FFC0FF", "#40FFFFFF", "#40C0FFFF", "#4080FFFF",
        "#4040FFFF", "#8040FFFF", "#C040FFFF", "#FF40FFFF"
    ];
```

Add this test:

```csharp
    /// <summary>
    /// Ensures all sixteen colored-cube PSP material settings preserve their authored base colors.
    /// </summary>
    [Fact]
    public void ColoredCubeGridMaterialSettings_preserve_all_psp_base_colors() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();

        for (int cubeIndex = 0; cubeIndex < CubeMaterialRelativePaths.Length; cubeIndex++) {
            string materialPath = Path.Combine(CityProjectRootPath, CubeMaterialRelativePaths[cubeIndex]);

            Assert.True(settingsService.TryLoad(materialPath, out MaterialAssetImportSettings settings));
            Assert.NotNull(settings);
            Assert.True(settings.Processor.Platforms.ContainsKey("psp"));
            Assert.Equal(
                ExpectedCubeMaterialColors[cubeIndex],
                settings.Processor.Platforms["psp"].FieldValues["base-color"]);
        }
    }
```

- [ ] **Step 2: Run the new regression and verify it passes**

Run:

```bash
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~CityColoredCubeGridSceneTests.ColoredCubeGridMaterialSettings_preserve_all_psp_base_colors -v minimal
```

Expected: `PASS`

- [ ] **Step 3: Expand the cooked-material regression to cover all sixteen cubes**

Replace the single-material cook assertion with this loop-driven test:

```csharp
    /// <summary>
    /// Ensures all sixteen colored cube materials cook into PSP base-color buffers that preserve the authored colors.
    /// </summary>
    [Fact]
    public void ColoredCubeGridMaterialCook_preserves_all_psp_base_colors() {
        MaterialAssetSettingsService settingsService = new MaterialAssetSettingsService();
        PspPlatformAssetBuilder builder = new PspPlatformAssetBuilder();

        for (int cubeIndex = 0; cubeIndex < CubeMaterialRelativePaths.Length; cubeIndex++) {
            string materialRelativePath = CubeMaterialRelativePaths[cubeIndex];
            string materialPath = Path.Combine(CityProjectRootPath, materialRelativePath);

            Assert.True(settingsService.TryLoad(materialPath, out MaterialAssetImportSettings settings));
            Assert.NotNull(settings);
            Assert.True(settings.Processor.Platforms.ContainsKey("psp"));

            MaterialAsset sourceMaterialAsset = ReadMaterialAsset(materialRelativePath);
            MaterialAssetProcessorSettings platformSettings = settings.Processor.Platforms["psp"];
            Dictionary<string, string> fieldValues = new Dictionary<string, string>(platformSettings.FieldValues) {
                ["shader-asset-id"] = sourceMaterialAsset.ShaderAssetId,
                ["vertex-program"] = sourceMaterialAsset.VertexProgram,
                ["pixel-program"] = sourceMaterialAsset.PixelProgram,
                ["variant"] = sourceMaterialAsset.Variant
            };

            PlatformMaterialCookResult cookResult = builder.CookMaterial(new PlatformMaterialCookRequest(
                materialRelativePath.Replace('\\', '/'),
                materialRelativePath.Replace('\\', '/'),
                "psp",
                "debug",
                "psp-forward",
                platformSettings.SchemaId,
                fieldValues));

            MaterialAsset cookedMaterialAsset = Assert.IsType<MaterialAsset>(AssetSerializer.DeserializeFromBytes(cookResult.CookedMaterialBytes));
            MaterialConstantBufferAsset baseColorBuffer = Assert.Single(
                cookedMaterialAsset.ConstantBuffers,
                buffer => buffer.Name == "BaseColorBuffer");

            float4 expectedColor = ParseExpectedColor(ExpectedCubeMaterialColors[cubeIndex]);
            Assert.Equal(expectedColor.X, BitConverter.ToSingle(baseColorBuffer.Data, 0), 5);
            Assert.Equal(expectedColor.Y, BitConverter.ToSingle(baseColorBuffer.Data, 4), 5);
            Assert.Equal(expectedColor.Z, BitConverter.ToSingle(baseColorBuffer.Data, 8), 5);
            Assert.Equal(expectedColor.W, BitConverter.ToSingle(baseColorBuffer.Data, 12), 5);
        }
    }
```

Add helpers near the bottom of the file:

```csharp
    /// <summary>
    /// Reads one authored material asset from disk.
    /// </summary>
    /// <param name="materialRelativePath">Project-relative authored material path.</param>
    /// <returns>Deserialized authored material asset.</returns>
    MaterialAsset ReadMaterialAsset(string materialRelativePath) {
        string materialPath = Path.Combine(CityProjectRootPath, materialRelativePath);
        Assert.True(File.Exists(materialPath));

        using FileStream stream = File.OpenRead(materialPath);
        return Assert.IsType<MaterialAsset>(EditorAssetBinarySerializer.Deserialize(stream));
    }

    /// <summary>
    /// Parses one expected #RRGGBBAA color string into normalized float channels.
    /// </summary>
    /// <param name="hexColor">Authored color string.</param>
    /// <returns>Normalized color.</returns>
    static float4 ParseExpectedColor(string hexColor) {
        Assert.StartsWith("#", hexColor, StringComparison.Ordinal);
        Assert.Equal(9, hexColor.Length);

        byte red = Convert.ToByte(hexColor.Substring(1, 2), 16);
        byte green = Convert.ToByte(hexColor.Substring(3, 2), 16);
        byte blue = Convert.ToByte(hexColor.Substring(5, 2), 16);
        byte alpha = Convert.ToByte(hexColor.Substring(7, 2), 16);

        return new float4(
            red / 255f,
            green / 255f,
            blue / 255f,
            alpha / 255f);
    }
```

- [ ] **Step 4: Run the focused colored-cube-grid builder suite**

Run:

```bash
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~CityColoredCubeGridSceneTests -v minimal
```

Expected: `PASS`

- [ ] **Step 5: Commit the test-only changes**

```bash
rtk git -C C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting add builder.tests/CityColoredCubeGridSceneTests.cs
rtk git -C C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting commit -m "Expand PSP colored cube material coverage"
```

### Task 2: Reproduce `colored_cube_grid` on the GPU path

**Files:**
- Modify temporarily during verification only: `C:\dev\helprojs\city\user_settings\build_config.json`
- Read: latest generated startup manifest under `C:\Users\Helena\AppData\Local\Temp\helengine-platform-build\psp\*\generated-core\runtime\runtime_startup_manifest.cpp`
- Read: `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`

- [ ] **Step 1: Temporarily point the PSP build config to `colored_cube_grid` only**

Run this exact temporary override script:

```powershell
$buildConfigPath = 'C:\dev\helprojs\city\user_settings\build_config.json'
$backupPath = 'C:\tmp\psp-colored-cubes-build-config-backup.json'
Copy-Item $buildConfigPath $backupPath -Force
try {
    $json = Get-Content $buildConfigPath -Raw | ConvertFrom-Json
    $psp = $json.platforms | Where-Object { $_.platformId -eq 'psp' } | Select-Object -First 1
    if ($null -eq $psp) {
        throw 'PSP platform config not found.'
    }

    $psp.selectedSceneIds = @('colored_cube_grid')
    $psp.sceneOrders = @(@{
        sceneId = 'colored_cube_grid'
        orderNumber = 1
    })
    $psp.outputDirectoryPath = 'C:\dev\helprojs\output\psp-colored-cubes-gpu'

    $json | ConvertTo-Json -Depth 10 | Set-Content $buildConfigPath
} finally {
}
```

Expected: `build_config.json` contains only `colored_cube_grid` for PSP until the restore step in Task 2 Step 7.

- [ ] **Step 2: Export a fresh PSP build from the editor path**

Run:

```bash
rtk dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city --build psp --output C:\dev\helprojs\output\psp-colored-cubes-gpu
```

Expected: `Build completed for platform 'psp': C:\dev\helprojs\output\psp-colored-cubes-gpu`

- [ ] **Step 3: Verify the generated startup manifest now points at `colored_cube_grid`**

Run:

```powershell
$root = 'C:\Users\Helena\AppData\Local\Temp\helengine-platform-build\psp'
$latest = Get-ChildItem $root -Directory | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
$manifestPath = Join-Path $latest.FullName 'generated-core\runtime\runtime_startup_manifest.cpp'
Get-Content $manifestPath
```

Expected line:

```cpp
static const char kRuntimeStartupSceneRelativePath[] = "cooked/scenes/rendering/colored_cube_grid.hasset";
```

- [ ] **Step 4: Rebuild the PSP GPU-lighting worktree with diagnostics enabled**

Run:

```powershell
$root = 'C:\Users\Helena\AppData\Local\Temp\helengine-platform-build\psp'
$latest = Get-ChildItem $root -Directory | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if ($null -eq $latest) {
    throw 'No PSP generated-core workspace was found.'
}

rtk docker run --rm -v C:/dev/helworks/helengine-psp/.worktrees/psp-cube-test-gpu-lighting:/workspace -v ($latest.FullName.Replace('\', '/') + '/generated-core'):/generated-core -w /workspace helengine-psp /bin/sh -lc "rm -rf build && mkdir build && cd build && /usr/local/pspdev/bin/psp-cmake -DHELENGINE_CORE_CPP_ROOT=/generated-core -DHELENGINE_PSP_ISOLATED_BOOT=ON -DHELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON -DHELENGINE_PSP_ENABLE_BOOT_TRACE=ON -DHELENGINE_PSP_ENABLE_RENDER_PROFILER=ON .. && make -j2"
```

Expected: build completes and creates `C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting\build\EBOOT.PBP`

- [ ] **Step 5: Stage the worktree build into PPSSPP and capture a boot log baseline**

Run:

```powershell
Copy-Item C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting\build\EBOOT.PBP C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP -Force
rtk powershell -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-psp\tools\run_ppsspp_boot_check.ps1
```

Expected:
- `helengine_psp_boot.log` exists at `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`
- the startup scene is `colored_cube_grid`
- the profiler shows repeated 3D draws for the scene

- [ ] **Step 6: Inspect the routing counters before changing renderer code**

Run:

```bash
rtk rg -n "LoadStartupScene id=colored_cube_grid|gpuDirectionalCount=|cpuFallbackCount=" C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log
```

Expected baseline:
- `LoadStartupScene id=colored_cube_grid`
- `gpuDirectionalCount=` should be non-zero for the cubes if the current path already routes there
- `cpuFallbackCount=` reveals whether the scene is silently falling back

- [ ] **Step 7: Restore the city PSP build config immediately after the baseline capture**

Run:

```powershell
Move-Item C:\tmp\psp-colored-cubes-build-config-backup.json C:\dev\helprojs\city\user_settings\build_config.json -Force
```

Expected: the city PSP build config returns to its normal demo-disc ordering.

### Task 3: Fix repeated GPU material color state if `colored_cube_grid` smears colors

**Files:**
- Modify: `C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting\src\platform\psp\rendering\PspRenderManager3D.cpp`
- Modify: `C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting\src\platform\psp\rendering\PspRenderManager3D.hpp` (only if a new helper declaration is needed)

- [ ] **Step 1: Add an explicit helper that binds per-draw GPU material color state**

If the baseline shows the scene is GPU-routed but the cubes do not keep distinct colors, add this helper declaration in `PspRenderManager3D.hpp` next to `ConfigureFixedFunctionMaterial(...)`:

```cpp
        /// Configures the per-draw GU color material state for GPU directional lighting.
        void ConfigureGpuDirectionalMaterialColor(const float4& baseColor);
```

Add this helper implementation in `PspRenderManager3D.cpp` near `ConfigureFixedFunctionMaterial(...)`:

```cpp
    /// Configures the per-draw GU color material state for GPU directional lighting.
    void PspRenderManager3D::ConfigureGpuDirectionalMaterialColor(const float4& baseColor) {
        const std::uint32_t baseColorAbgr = ConvertColorToAbgr(baseColor);

        sceGuColor(baseColorAbgr);
        sceGuColorMaterial(GU_AMBIENT | GU_DIFFUSE);
        sceGuAmbientColor(baseColorAbgr);
        sceGuModelColor(0, baseColorAbgr, baseColorAbgr, 0);
    }
```

- [ ] **Step 2: Route GPU-lit draws through the explicit per-draw color helper**

Change `ConfigureFixedFunctionMaterial(...)` in `PspRenderManager3D.cpp` to call the new helper before lighting-state toggles:

```cpp
    void PspRenderManager3D::ConfigureFixedFunctionMaterial(const float4& baseColor, bool useLighting) {
        ConfigureGpuDirectionalMaterialColor(baseColor);

        if (!useLighting) {
            SetLight0Enabled(false);
            SetLightingEnabled(false);
            return;
        }

        SetLightingEnabled(true);
        SetLight0Enabled(CurrentLighting.HasDirectionalLight);
    }
```

If this exact change is not enough, keep the fix inside this same material-state setup region. Do not add scene-name checks and do not touch authored material data.

- [ ] **Step 3: Rebuild the worktree and rerun the same `colored_cube_grid` startup capture**

Run the same commands from Task 2 Step 4 and Step 5, using the latest fresh generated-core workspace for `colored_cube_grid`.

Expected:
- build succeeds
- PPSSPP boots `colored_cube_grid`
- `gpuDirectionalCount` stays non-zero
- `cpuFallbackCount` stays `0`

- [ ] **Step 4: Verify the user-facing result**

Ask the user to inspect the running scene and confirm:

- the cubes have distinct colors
- the colors stay stable while rotating
- the scene is still on the GPU path

Expected: the user confirms the per-cube colors are preserved.

- [ ] **Step 5: Commit the renderer fix**

```bash
rtk git -C C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting add src/platform/psp/rendering/PspRenderManager3D.cpp src/platform/psp/rendering/PspRenderManager3D.hpp
rtk git -C C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting commit -m "Preserve PSP GPU cube material colors"
```

### Task 4: Final verification and handoff

**Files:**
- Read: `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`
- Read: `C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting\src\platform\psp\rendering\PspRenderManager3D.cpp`

- [ ] **Step 1: Re-run the focused builder tests**

Run:

```bash
rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter FullyQualifiedName~CityColoredCubeGridSceneTests -v minimal
```

Expected: `PASS`

- [ ] **Step 2: Confirm the runtime log still shows GPU routing and no CPU fallback**

Run:

```bash
rtk rg -n "LoadStartupScene id=colored_cube_grid|gpuDirectionalCount=|cpuFallbackCount=" C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log
```

Expected:
- startup scene is `colored_cube_grid`
- `gpuDirectionalCount` is non-zero
- `cpuFallbackCount=0`

- [ ] **Step 3: Capture the final worktree state**

Run:

```bash
rtk git -C C:\dev\helworks\helengine-psp\.worktrees\psp-cube-test-gpu-lighting status --short
```

Expected: only the intended committed plan/tests/renderer changes remain, with no temporary config file edits left behind.

- [ ] **Step 4: Summarize the result for the user**

Report:

- whether `colored_cube_grid` now stays on the GPU path
- whether all `16` colors are preserved
- what renderer files changed
- what the focused tests and runtime verification showed
