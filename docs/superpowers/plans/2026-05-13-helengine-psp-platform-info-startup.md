# Helengine PSP PlatformInfo Startup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make PSP boot directly into the authored `textured_cube_grid` startup scene and satisfy the current engine `PlatformInfo` initialization contract using the same native manifest and runtime seam that Windows already uses.

**Architecture:** First add a PSP native manifest writer that embeds the startup-scene path plus runtime platform name/version into generated-core runtime source. Then wire the PSP builder to rewrite those files from the build manifest and update `PspBootHost` to construct `PlatformInfo` from the embedded runtime functions and pass it into the five-argument `Core::Initialize(...)` call. Verification stays honest: builder tests first, then inspect the generated startup manifest, then rebuild `EBOOT.PBP` with runtime startup enabled.

**Tech Stack:** C#, xUnit, generated native manifest source emission, C++ PSP runtime host code, Dockerized PSP native build via `make`

---

### Task 1: Add A PSP Runtime Native Manifest Writer

**Files:**
- Create: `builder/PspRuntimeNativeManifestWriter.cs`
- Create: `builder.tests/PspRuntimeNativeManifestWriterTests.cs`
- Reference: `C:\dev\helworks\helengine-windows\builder\WindowsRuntimeNativeManifestWriter.cs`

- [ ] **Step 1: Write the failing manifest-writer test**

```csharp
using helengine.baseplatform.Manifest;

namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the PSP runtime native manifest writer embeds startup-scene and platform metadata into generated-core runtime source.
/// </summary>
public sealed class PspRuntimeNativeManifestWriterTests {
    /// <summary>
    /// Ensures the PSP writer emits startup-scene, platform-name, and platform-version functions into the generated startup manifest.
    /// </summary>
    [Fact]
    public void Write_embeds_startup_scene_and_platform_metadata() {
        string workingRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string runtimeRoot = Path.Combine(generatedCoreRoot, "runtime");
        Directory.CreateDirectory(runtimeRoot);

        PlatformBuildManifest manifest = new(
            3,
            "city",
            "psp",
            "1.0.0",
            "scenes/rendering/textured_cube_grid.helen",
            [
                new PlatformBuildScene(
                    "scenes/rendering/textured_cube_grid.helen",
                    "Textured Cube Grid",
                    "cooked/scenes/rendering/textured_cube_grid.hasset",
                    [],
                    [new KeyValuePair<string, string>("cooked-relative-path", "cooked/scenes/rendering/textured_cube_grid.hasset")])
            ],
            Array.Empty<PlatformBuildAsset>(),
            [
                new PlatformBuildArtifact("cooked/scenes/rendering/textured_cube_grid.hasset", "scene:textured-cube-grid", "sha256:scene", "scene", "shared")
            ],
            Array.Empty<PlatformBuildCodeModule>(),
            Array.Empty<PlatformArtifactPlacement>(),
            new PlatformContainerWritePlan("psp-homebrew", Array.Empty<PlatformContainerArtifact>()));

        new PspRuntimeNativeManifestWriter().Write(generatedCoreRoot, manifest);

        string headerContents = File.ReadAllText(Path.Combine(runtimeRoot, "runtime_startup_manifest.hpp"));
        string sourceContents = File.ReadAllText(Path.Combine(runtimeRoot, "runtime_startup_manifest.cpp"));

        Assert.Contains("const char* he_get_runtime_startup_scene_relative_path();", headerContents);
        Assert.Contains("const char* he_get_runtime_platform_name();", headerContents);
        Assert.Contains("const char* he_get_runtime_platform_version();", headerContents);
        Assert.Contains("cooked/scenes/rendering/textured_cube_grid.hasset", sourceContents);
        Assert.Contains("static const char kRuntimePlatformName[] = \"psp\";", sourceContents);
        Assert.Contains("static const char kRuntimePlatformVersion[] = \"1.0.0\";", sourceContents);
    }
}
```

- [ ] **Step 2: Run the new test to verify it fails**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~PspRuntimeNativeManifestWriterTests" -v minimal`

Expected: FAIL because `PspRuntimeNativeManifestWriter` does not exist yet.

- [ ] **Step 3: Write the minimal PSP manifest writer**

```csharp
using System.Text;
using helengine.baseplatform.Manifest;

namespace helengine.psp.builder;

/// <summary>
/// Writes generated C++ runtime manifest source files consumed by the PSP native player.
/// </summary>
public sealed class PspRuntimeNativeManifestWriter {
    /// <summary>
    /// Writes the PSP startup and scene-catalog runtime manifests into the generated-core runtime folder.
    /// </summary>
    /// <param name="generatedCoreRootPath">Absolute generated-core root path.</param>
    /// <param name="manifest">Resolved build manifest whose runtime metadata should be embedded.</param>
    public void Write(string generatedCoreRootPath, PlatformBuildManifest manifest) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        } else if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        }

        string runtimeRootPath = Path.Combine(generatedCoreRootPath, "runtime");
        Directory.CreateDirectory(runtimeRootPath);

        File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_startup_manifest.hpp"), BuildStartupManifestHeaderContents());
        File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_startup_manifest.cpp"), BuildStartupManifestSourceContents(manifest));
        File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_scene_catalog_manifest.hpp"), BuildSceneCatalogManifestHeaderContents());
        File.WriteAllText(Path.Combine(runtimeRootPath, "runtime_scene_catalog_manifest.cpp"), BuildSceneCatalogManifestSourceContents(manifest));
    }

    static string BuildStartupManifestHeaderContents() {
        StringBuilder builder = new();
        builder.AppendLine("#pragma once");
        builder.AppendLine();
        builder.AppendLine("const char* he_get_runtime_startup_scene_relative_path();");
        builder.AppendLine("const char* he_get_runtime_platform_name();");
        builder.AppendLine("const char* he_get_runtime_platform_version();");
        return builder.ToString();
    }

    static string BuildStartupManifestSourceContents(PlatformBuildManifest manifest) {
        string startupSceneRelativePath = ResolveStartupSceneRelativePath(manifest);

        StringBuilder builder = new();
        builder.AppendLine("#include \"runtime/runtime_startup_manifest.hpp\"");
        builder.AppendLine();
        builder.AppendLine("static const char kRuntimeStartupSceneRelativePath[] = \"" + EscapeCppStringLiteral(startupSceneRelativePath) + "\";");
        builder.AppendLine("static const char kRuntimePlatformName[] = \"" + EscapeCppStringLiteral(manifest.PlatformName) + "\";");
        builder.AppendLine("static const char kRuntimePlatformVersion[] = \"" + EscapeCppStringLiteral(manifest.PlatformVersion) + "\";");
        builder.AppendLine();
        builder.AppendLine("const char* he_get_runtime_startup_scene_relative_path() {");
        builder.AppendLine("    return kRuntimeStartupSceneRelativePath;");
        builder.AppendLine("}");
        builder.AppendLine();
        builder.AppendLine("const char* he_get_runtime_platform_name() {");
        builder.AppendLine("    return kRuntimePlatformName;");
        builder.AppendLine("}");
        builder.AppendLine();
        builder.AppendLine("const char* he_get_runtime_platform_version() {");
        builder.AppendLine("    return kRuntimePlatformVersion;");
        builder.AppendLine("}");
        return builder.ToString();
    }
}
```

Also copy the existing Windows implementations of:

- `BuildSceneCatalogManifestHeaderContents()`
- `BuildSceneCatalogManifestSourceContents(PlatformBuildManifest manifest)`
- `ResolveStartupSceneRelativePath(PlatformBuildManifest manifest)`
- `ResolveCookedRelativePath(PlatformBuildScene scene)`
- `EscapeCppStringLiteral(string value)`

from [WindowsRuntimeNativeManifestWriter.cs](C:\dev\helworks\helengine-windows\builder\WindowsRuntimeNativeManifestWriter.cs), renaming the class to `PspRuntimeNativeManifestWriter` only. Do not add window-settings or code-module-writing logic to the PSP writer in this task.

- [ ] **Step 4: Run the manifest-writer tests**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~PspRuntimeNativeManifestWriterTests" -v minimal`

Expected: PASS

- [ ] **Step 5: Commit**

```bash
rtk git add builder/PspRuntimeNativeManifestWriter.cs builder.tests/PspRuntimeNativeManifestWriterTests.cs
rtk git commit -m "Add PSP runtime native manifest writer"
```

### Task 2: Rewrite PSP Generated Runtime Manifests During Build

**Files:**
- Modify: `builder/PspPlatformAssetBuilder.cs`
- Modify: `builder.tests/PspPlatformAssetBuilderTests.cs`

- [ ] **Step 1: Add a failing builder integration test for manifest rewriting**

```csharp
/// <summary>
/// Verifies the PSP builder rewrites runtime manifest sources so startup-scene and platform metadata match the current build manifest.
/// </summary>
[Fact]
public async Task BuildAsync_rewrites_runtime_startup_manifest_with_platform_metadata() {
    string workingRoot = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
    string outputRoot = Path.Combine(workingRoot, "out");
    string sourceRoot = Path.Combine(workingRoot, "staging");
    string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
    string generatedCoreRuntimeRoot = Path.Combine(generatedCoreRoot, "runtime");
    string repositoryRoot = Path.Combine(workingRoot, "repository");
    string sceneSourcePath = Path.Combine(sourceRoot, "cooked", "scenes", "rendering", "textured_cube_grid.hasset");

    Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath)!);
    Directory.CreateDirectory(generatedCoreRuntimeRoot);
    Directory.CreateDirectory(Path.Combine(repositoryRoot, "src", "platform", "psp"));

    File.WriteAllText(sceneSourcePath, "scene payload");
    File.WriteAllText(Path.Combine(generatedCoreRoot, "helengine_core_amalgamated.cpp"), "// generated");
    File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "#pragma once");
    File.WriteAllText(Path.Combine(generatedCoreRuntimeRoot, "runtime_startup_manifest.cpp"), "// stale startup");
    File.WriteAllText(Path.Combine(generatedCoreRuntimeRoot, "runtime_scene_catalog_manifest.cpp"), "// stale scene catalog");
    File.WriteAllText(Path.Combine(repositoryRoot, "Makefile"), "# fake");
    File.WriteAllText(Path.Combine(repositoryRoot, "src", "platform", "psp", "PspBootHost.cpp"), "// fake");

    string previousDirectory = Directory.GetCurrentDirectory();
    string previousRepositoryRoot = Environment.GetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT");
    try {
        Directory.SetCurrentDirectory(sourceRoot);
        Environment.SetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT", repositoryRoot);

        PlatformBuildManifest manifest = new(
            3,
            "city",
            "psp",
            "1.0.0",
            "scenes/rendering/textured_cube_grid.helen",
            [
                new PlatformBuildScene(
                    "scenes/rendering/textured_cube_grid.helen",
                    "Textured Cube Grid",
                    "cooked/scenes/rendering/textured_cube_grid.hasset",
                    [],
                    [new KeyValuePair<string, string>("cooked-relative-path", "cooked/scenes/rendering/textured_cube_grid.hasset")])
            ],
            Array.Empty<PlatformBuildAsset>(),
            [
                new PlatformBuildArtifact("cooked/scenes/rendering/textured_cube_grid.hasset", "scene:textured-cube-grid", "sha256:scene", "scene", "shared")
            ],
            Array.Empty<PlatformBuildCodeModule>(),
            Array.Empty<PlatformArtifactPlacement>(),
            new PlatformContainerWritePlan("psp-homebrew", Array.Empty<PlatformContainerArtifact>()));

        PlatformBuildRequest request = new(
            manifest,
            [new PlatformBuildTargetVariant("psp-debug", "psp", "psp", "default")],
            [new PlatformCookProfile(
                "default",
                "Default",
                new PlatformCookProfileCapabilities(
                    "psp",
                    "raw",
                    "rgba",
                    "psp-scene-v1",
                    PlatformSerializationEndianness.LittleEndian))],
            outputRoot,
            Path.Combine(workingRoot, "tmp"),
            selectedBuildProfileId: "debug",
            selectedGraphicsProfileId: "psp-forward",
            selectedCodegenProfileId: "default",
            selectedBuildOptionValues: new Dictionary<string, string>(),
            selectedGraphicsOptionValues: new Dictionary<string, string>(),
            selectedCodegenOptionValues: new Dictionary<string, string>(),
            generatedCoreCppRootPath: generatedCoreRoot,
            selectedMediaProfileId: "psp-game-folder",
            selectedStorageProfileId: "homebrew-app");

        PspPlatformAssetBuilder builder = new(new FakePspNativeBuildExecutor());

        await builder.BuildAsync(
            request,
            new RecordingProgressReporter(),
            new RecordingDiagnosticReporter(),
            CancellationToken.None);

        string startupManifestContents = File.ReadAllText(Path.Combine(generatedCoreRuntimeRoot, "runtime_startup_manifest.cpp"));
        Assert.Contains("cooked/scenes/rendering/textured_cube_grid.hasset", startupManifestContents);
        Assert.Contains("static const char kRuntimePlatformName[] = \"psp\";", startupManifestContents);
        Assert.Contains("static const char kRuntimePlatformVersion[] = \"1.0.0\";", startupManifestContents);
    } finally {
        Directory.SetCurrentDirectory(previousDirectory);
        Environment.SetEnvironmentVariable("HELENGINE_PSP_REPOSITORY_ROOT", previousRepositoryRoot);
        if (Directory.Exists(workingRoot)) {
            Directory.Delete(workingRoot, true);
        }
    }
}
```

- [ ] **Step 2: Run the focused builder test to verify it fails**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~BuildAsync_rewrites_runtime_startup_manifest_with_platform_metadata" -v minimal`

Expected: FAIL because `BuildAsync(...)` currently preserves the stale manifest source.

- [ ] **Step 3: Integrate the new writer into the PSP builder**

```csharp
/// <summary>
/// Writes generated runtime manifest source files consumed by the PSP native player.
/// </summary>
readonly PspRuntimeNativeManifestWriter RuntimeNativeManifestWriter;

public PspPlatformAssetBuilder(IPspNativeBuildExecutor nativeBuildExecutor) {
    NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
    AppLayoutWriter = new PspAppLayoutWriter();
    GeneratedCoreCompatibilityNormalizer = new PspGeneratedCoreCompatibilityNormalizer();
    RuntimeNativeManifestWriter = new PspRuntimeNativeManifestWriter();
    Descriptor = new PlatformBuilderDescriptor(
        "helengine.psp.builder",
        "1.0.0",
        "psp",
        new EngineCompatibilityRange("1.0.0", "999.0.0"),
        new ManifestCompatibilityRange(1, 3),
        ["psp"],
        ["debug"]);
    Definition = PspPlatformDefinitionFactory.Create();
}

public Task<PlatformBuildReport> BuildAsync(
    PlatformBuildRequest request,
    IPlatformBuildProgressReporter progressReporter,
    IPlatformBuildDiagnosticReporter diagnosticReporter,
    CancellationToken cancellationToken) {
    ValidateRequest(request);
    if (request == null) {
        throw new ArgumentNullException(nameof(request));
    } else if (progressReporter == null) {
        throw new ArgumentNullException(nameof(progressReporter));
    } else if (diagnosticReporter == null) {
        throw new ArgumentNullException(nameof(diagnosticReporter));
    }

    Directory.CreateDirectory(request.OutputRoot);
    Directory.CreateDirectory(request.WorkingRoot);

    List<PlatformBuildDiagnostic> diagnostics = [];
    List<PlatformBuildItemOutcome> sceneOutcomes = BuildSceneOutcomes(request.Manifest.Scenes);
    List<PlatformBuildItemOutcome> looseAssetOutcomes = BuildLooseAssetOutcomes(request.Manifest.LooseAssets);
    string stagingRootPath = Path.Combine(request.WorkingRoot, "psp-staging");

    RuntimeNativeManifestWriter.Write(request.GeneratedCoreCppRootPath, request.Manifest);
    ResetDirectory(stagingRootPath);
    Directory.CreateDirectory(stagingRootPath);
    StageCookedArtifacts(request, stagingRootPath, diagnostics, diagnosticReporter, progressReporter, cancellationToken);

    if (diagnostics.Count == 0) {
        GeneratedCoreCompatibilityNormalizer.Normalize(request.GeneratedCoreCppRootPath);
        PspBuildWorkspace workspace = CreateWorkspace(request, stagingRootPath);
        NativeBuildExecutor.Build(workspace, cancellationToken);
        AppLayoutWriter.Write(workspace);
        VerifyPackagedOutputs(workspace);
    }

    bool succeeded = diagnostics.Count == 0;
    return Task.FromResult(new PlatformBuildReport(
        succeeded,
        [.. diagnostics],
        [.. sceneOutcomes],
        [.. looseAssetOutcomes]));
}
```

- [ ] **Step 4: Run the PSP builder tests**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~PspPlatformAssetBuilderTests|FullyQualifiedName~PspRuntimeNativeManifestWriterTests" -v minimal`

Expected: PASS

- [ ] **Step 5: Commit**

```bash
rtk git add builder/PspPlatformAssetBuilder.cs builder.tests/PspPlatformAssetBuilderTests.cs
rtk git commit -m "Rewrite PSP runtime manifests from build output"
```

### Task 3: Inject PlatformInfo Into PSP Core Initialization

**Files:**
- Modify: `src/platform/psp/PspBootHost.hpp`
- Modify: `src/platform/psp/PspBootHost.cpp`
- Reference: `C:\dev\helworks\helengine-windows\src\platform\windows\win32\win32_application.cpp`

- [ ] **Step 1: Rebuild the PSP player against the newest generated-core root to capture the current red state**

```powershell
$generatedCoreRoot = Get-ChildItem "$env:LOCALAPPDATA\Temp\helengine-platform-build\psp" -Directory |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1 |
    ForEach-Object { Join-Path $_.FullName 'generated-core' }

$generatedCoreDockerPath = ($generatedCoreRoot -replace '\\', '/')

rtk proxy docker run --rm `
  -v C:/dev/helworks/helengine-psp:/workspace `
  -v ${generatedCoreDockerPath}:/generated-core `
  -w /workspace `
  helengine-psp `
  make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

Expected: FAIL with the PSP host still using the old four-argument `Core::Initialize(...)` shape or otherwise missing `PlatformInfo` integration.

- [ ] **Step 2: Add the PSP PlatformInfo factory method to the boot-host header**

```cpp
#pragma once

#include <string>

class Core;
class CoreInitializationOptions;
class PlatformInfo;
class RenderManager2D;
class RenderManager3D;
class IInputBackend;
class SceneAsset;

namespace helengine::psp {
    /// Owns the PSP native boot flow, generated-core initialization, and frame presentation loop.
    class PspBootHost {
    public:
        /// Creates the PSP boot host with no initialized graphics or engine state.
        explicit PspBootHost(const std::string& executablePath);

        /// Initializes the PSP runtime and presents frames until shutdown.
        int Run();

    private:
        /// Builds the runtime platform metadata embedded into the PSP generated startup manifest.
        PlatformInfo* BuildRuntimePlatformInfo();
```

- [ ] **Step 3: Inject PlatformInfo in the PSP boot-host implementation**

```cpp
#include "platform/psp/PspBootHost.hpp"

#include <cstdio>
#include <exception>
#include <sstream>
#include <string>

#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "PlatformInfo.hpp"
#include "RenderManager2D.hpp"
#include "RenderManager3D.hpp"
#include "SceneAsset.hpp"
#include "platform/psp/PspAppRootPathResolver.hpp"
#include "platform/psp/PspBootTrace.hpp"
#include "platform/psp/PspInputBackend.hpp"
#include "platform/psp/PspPackagedAssetLoader.hpp"
#include "platform/psp/rendering/PspRenderManager2D.hpp"
#include "platform/psp/rendering/PspRenderManager3D.hpp"
#include "runtime/native_exceptions.hpp"
#include "runtime/runtime_startup_manifest.hpp"

void PspBootHost::InitializeCore(const std::string& appRootPath) {
    EnterBootStage(RuntimeCoreInitializationStageName);

    EngineCore = new Core();
    EngineOptions = EngineCore->get_InitializationOptions();
    EngineOptions->set_ContentRootPath(appRootPath);
    EngineOptions->set_UpdateOrderLayers(4);
    EngineOptions->set_RenderOrderLayers3D(4);
    EngineOptions->set_UpdateListInitialCapacity(64);
    EngineOptions->set_RenderList2DInitialCapacity(8);
    EngineOptions->set_RenderList3DInitialCapacity(64);

    rendering::PspRenderManager3D* pspRenderManager3D = new rendering::PspRenderManager3D();
    rendering::PspRenderManager2D* pspRenderManager2D = new rendering::PspRenderManager2D();
    pspRenderManager3D->SetRenderManager2D(pspRenderManager2D);
    EngineRenderManager3D = pspRenderManager3D;
    EngineRenderManager2D = pspRenderManager2D;
    EngineInputBackend = new PspInputBackend();

    EngineRenderManager3D->AddWindow(0, ScreenWidth, ScreenHeight);

    PlatformInfo* platformInfo = BuildRuntimePlatformInfo();
    EngineCore->Initialize(
        EngineRenderManager3D,
        EngineRenderManager2D,
        EngineInputBackend,
        platformInfo,
        EngineOptions);

    CompleteBootStage();
}

PlatformInfo* PspBootHost::BuildRuntimePlatformInfo() {
    const char* platformName = he_get_runtime_platform_name();
    if (platformName == nullptr || platformName[0] == '\0') {
        throw std::runtime_error("Packaged runtime platform name was not embedded into the PSP build.");
    }

    const char* platformVersion = he_get_runtime_platform_version();
    if (platformVersion == nullptr || platformVersion[0] == '\0') {
        throw std::runtime_error("Packaged runtime platform version was not embedded into the PSP build.");
    }

    PspBootTrace::WriteLine(std::string("Runtime platform info resolved to '") + platformName + "' version '" + platformVersion + "'.");
    return new PlatformInfo(std::string(platformName), std::string(platformVersion));
}
```

- [ ] **Step 4: Rebuild the PSP player and inspect the generated startup manifest**

Run the native build again:

```powershell
$generatedCoreRoot = Get-ChildItem "$env:LOCALAPPDATA\Temp\helengine-platform-build\psp" -Directory |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1 |
    ForEach-Object { Join-Path $_.FullName 'generated-core' }

Get-Content (Join-Path $generatedCoreRoot 'runtime\runtime_startup_manifest.cpp')

$generatedCoreDockerPath = ($generatedCoreRoot -replace '\\', '/')

rtk proxy docker run --rm `
  -v C:/dev/helworks/helengine-psp:/workspace `
  -v ${generatedCoreDockerPath}:/generated-core `
  -w /workspace `
  helengine-psp `
  make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

Expected:
- `runtime_startup_manifest.cpp` contains `cooked/scenes/rendering/textured_cube_grid.hasset`
- `runtime_startup_manifest.cpp` contains `kRuntimePlatformName[] = "psp"`
- `runtime_startup_manifest.cpp` contains `kRuntimePlatformVersion[] = "1.0.0"`
- Docker build succeeds and emits `build/EBOOT.PBP`

- [ ] **Step 5: Commit**

```bash
rtk git add src/platform/psp/PspBootHost.hpp src/platform/psp/PspBootHost.cpp
rtk git commit -m "Inject PSP platform info during core startup"
```

### Task 4: Final Verification In PPSSPP

**Files:**
- Verify: `build/EBOOT.PBP`
- Verify: `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP`
- Verify: `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`

- [ ] **Step 1: Run the full PSP builder test suite**

Run: `rtk dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS

- [ ] **Step 2: Copy the rebuilt EBOOT into the PPSSPP memstick tree**

```powershell
$memstickRoot = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE'
New-Item -ItemType Directory -Path $memstickRoot -Force | Out-Null
Copy-Item 'C:\dev\helworks\helengine-psp\build\EBOOT.PBP' (Join-Path $memstickRoot 'EBOOT.PBP') -Force
```

- [ ] **Step 3: Launch PPSSPP against the rebuilt PSP player**

```powershell
$ppssppExe = 'C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe'
$ebootPath = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP'
Start-Process -FilePath $ppssppExe -ArgumentList $ebootPath -WindowStyle Hidden
```

- [ ] **Step 4: Read the PSP boot log**

Run: `Get-Content 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log' -Tail 80`

Expected:
- the log includes the runtime platform info message for `psp` version `1.0.0`
- the log reaches `Stage complete RuntimeMainLoop`
- no startup failure mentions a missing platform name or platform version

- [ ] **Step 5: Commit**

```bash
rtk git status --short
```

Expected: only the planned PSP builder/runtime changes remain staged or committed, with no surprise file edits.

---

## Self-Review

- Spec coverage:
  - direct `textured_cube_grid` startup: covered by Task 1 and Task 2 manifest-writing steps plus Task 3 manifest inspection
  - PSP `PlatformInfo` values `psp` / `1.0.0`: covered by Task 1 writer output and Task 3 runtime injection
  - Windows-style seam parity: covered by Task 1 Windows-writer reference and Task 3 `BuildRuntimePlatformInfo()` parity
  - runtime main-loop verification: covered by Task 4 boot-log verification
- Placeholder scan:
  - removed external city config edits because no current local config file exists in this workspace
  - replaced unknown generated-core paths with a concrete PowerShell discovery command
- Type consistency:
  - `PlatformInfo* BuildRuntimePlatformInfo()` matches the Windows reference seam
  - `EngineCore->Initialize(..., platformInfo, EngineOptions)` consistently uses the five-argument shape
