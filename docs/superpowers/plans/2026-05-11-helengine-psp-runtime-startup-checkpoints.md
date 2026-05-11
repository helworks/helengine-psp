# Helengine PSP Runtime Startup Checkpoints Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an explicit PSP runtime-startup mode that advances through app-root resolution, `Core` initialization, packaged startup-scene load, and scene materialization with stage-specific diagnostics, while keeping isolated blank-frame boot as the default build path.

**Architecture:** Keep the current generated-core build seam unchanged and preserve `HELENGINE_PSP_ISOLATED_BOOT=ON` as the default builder path. Introduce a second explicit native build flag for checkpointed runtime startup, thread it through the Docker, Make, and CMake layers, and let `PspBootHost` route into a staged startup flow that records the active checkpoint and halts on-screen with the failing stage name if anything breaks. The staged flow should only enter the normal frame loop after startup-scene materialization succeeds.

**Tech Stack:** .NET 9, xUnit, Docker, GNU Make, CMake, C++17, PSPDEV/PSPSDK, PPSSPP

---

## File Structure

- Modify: `builder.tests/PspNativeBuildExecutorTests.cs`
  Purpose: lock the native Docker command contract so isolated boot stays the default and runtime startup remains explicitly disabled unless requested elsewhere.
- Modify: `builder/PspNativeBuildExecutor.cs`
  Purpose: pass the new runtime-startup flag value explicitly in the default native build command.
- Modify: `Makefile`
  Purpose: expose the runtime-startup flag to manual builds and forward it into CMake.
- Modify: `CMakeLists.txt`
  Purpose: add a runtime-startup option and compile definition without disturbing the generated-core source list.
- Modify: `src/platform/psp/PspBootHost.hpp`
  Purpose: declare the staged runtime-startup methods, stage tracking state, and scene forward declaration.
- Modify: `src/platform/psp/PspBootHost.cpp`
  Purpose: implement the checkpointed startup flow, stage-aware diagnostics, and explicit boot-mode routing.
- Modify: `README.md`
  Purpose: document the isolated baseline build, the explicit runtime-startup bring-up build, and the expected PPSSPP outcomes for both modes.

### Task 1: Lock The Default Native Build Mode Contract

**Files:**
- Modify: `builder.tests/PspNativeBuildExecutorTests.cs`
- Modify: `builder/PspNativeBuildExecutor.cs`

- [ ] **Step 1: Write the failing builder regression test for the explicit runtime-startup default**

Replace `builder.tests/PspNativeBuildExecutorTests.cs` with:

```csharp
namespace helengine.psp.builder.tests;

/// <summary>
/// Verifies the Docker command shape used by the PSP native-build executor.
/// </summary>
public sealed class PspNativeBuildExecutorTests {
    /// <summary>
    /// Ensures the native-build executor mounts the repository and generated core roots and forwards the generated-core path to make.
    /// </summary>
    [Fact]
    public void CreateBuildArguments_whenWorkspaceIsProvided_mountsRepositoryAndGeneratedCore() {
        PspBuildWorkspace workspace = new(
            "/repo",
            "/repo/tmp/psp-staging",
            "/generated-core",
            "/out",
            "/repo/build/EBOOT.PBP");

        IReadOnlyList<string> arguments = PspNativeBuildExecutor.CreateBuildArguments(workspace);

        Assert.Equal("run", arguments[0]);
        Assert.Contains("-v", arguments);
        Assert.Contains($"{workspace.RepositoryRootPath}:/workspace", arguments);
        Assert.Contains($"{workspace.GeneratedCoreRootPath}:/generated-core", arguments);
        Assert.Contains("helengine-psp", arguments);
        Assert.Contains("make", arguments);
        Assert.Contains("HELENGINE_CORE_CPP_ROOT=/generated-core", arguments);
    }

    /// <summary>
    /// Ensures the native-build executor enables isolated boot explicitly so the PSP player can stay on a blank frame during runtime bring-up.
    /// </summary>
    [Fact]
    public void CreateBuildArguments_whenWorkspaceIsProvided_enables_isolated_boot() {
        PspBuildWorkspace workspace = new(
            "/repo",
            "/repo/tmp/psp-staging",
            "/generated-core",
            "/out",
            "/repo/build/EBOOT.PBP");

        IReadOnlyList<string> arguments = PspNativeBuildExecutor.CreateBuildArguments(workspace);

        Assert.Contains("HELENGINE_PSP_ISOLATED_BOOT=ON", arguments);
    }

    /// <summary>
    /// Ensures the native-build executor keeps checkpointed runtime startup disabled unless a bring-up build opts in explicitly.
    /// </summary>
    [Fact]
    public void CreateBuildArguments_whenWorkspaceIsProvided_disables_runtime_startup_by_default() {
        PspBuildWorkspace workspace = new(
            "/repo",
            "/repo/tmp/psp-staging",
            "/generated-core",
            "/out",
            "/repo/build/EBOOT.PBP");

        IReadOnlyList<string> arguments = PspNativeBuildExecutor.CreateBuildArguments(workspace);

        Assert.Contains("HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF", arguments);
    }
}
```

- [ ] **Step 2: Run the targeted test to verify the new assertion fails**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~CreateBuildArguments_whenWorkspaceIsProvided_disables_runtime_startup_by_default" -v minimal`

Expected: FAIL because `PspNativeBuildExecutor.CreateBuildArguments(...)` does not yet include `HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF`.

- [ ] **Step 3: Update the default native-build command to carry the explicit runtime-startup flag**

Replace `builder/PspNativeBuildExecutor.cs` with:

```csharp
using System.Diagnostics;

namespace helengine.psp.builder;

/// <summary>
/// Invokes the Docker-based native PSP build using the staged generated-core root.
/// </summary>
public sealed class PspNativeBuildExecutor : IPspNativeBuildExecutor {
    /// <summary>
    /// Executes the native PSP build for one prepared workspace.
    /// </summary>
    /// <param name="workspace">Prepared PSP build workspace.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the native build.</param>
    public void Build(PspBuildWorkspace workspace, CancellationToken cancellationToken) {
        if (workspace == null) {
            throw new ArgumentNullException(nameof(workspace));
        }

        ProcessStartInfo startInfo = CreateStartInfo(workspace);
        using Process process = Process.Start(startInfo) ?? throw new InvalidOperationException("Failed to start the PSP native build.");
        using CancellationTokenRegistration cancellationRegistration = cancellationToken.Register(() => TryKillProcess(process));

        process.WaitForExit();
        cancellationToken.ThrowIfCancellationRequested();
        if (process.ExitCode != 0) {
            throw new InvalidOperationException($"The PSP native build failed with exit code {process.ExitCode}.");
        }
    }

    /// <summary>
    /// Creates the Docker process start-info for one prepared workspace.
    /// </summary>
    /// <param name="workspace">Prepared PSP build workspace.</param>
    /// <returns>Docker process start-info for the native PSP build.</returns>
    public static ProcessStartInfo CreateStartInfo(PspBuildWorkspace workspace) {
        if (workspace == null) {
            throw new ArgumentNullException(nameof(workspace));
        }

        ProcessStartInfo startInfo = new ProcessStartInfo {
            FileName = "docker",
            UseShellExecute = false,
            WorkingDirectory = workspace.RepositoryRootPath
        };

        IReadOnlyList<string> arguments = CreateBuildArguments(workspace);
        for (int index = 0; index < arguments.Count; index++) {
            startInfo.ArgumentList.Add(arguments[index]);
        }

        return startInfo;
    }

    /// <summary>
    /// Creates the Docker command arguments for one prepared workspace.
    /// </summary>
    /// <param name="workspace">Prepared PSP build workspace.</param>
    /// <returns>Ordered Docker command arguments.</returns>
    public static IReadOnlyList<string> CreateBuildArguments(PspBuildWorkspace workspace) {
        if (workspace == null) {
            throw new ArgumentNullException(nameof(workspace));
        }

        return [
            "run",
            "--rm",
            "-v",
            $"{workspace.RepositoryRootPath}:/workspace",
            "-v",
            $"{workspace.GeneratedCoreRootPath}:/generated-core",
            "-w",
            "/workspace",
            "helengine-psp",
            "make",
            "HELENGINE_CORE_CPP_ROOT=/generated-core",
            "HELENGINE_PSP_ISOLATED_BOOT=ON",
            "HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF"
        ];
    }

    /// <summary>
    /// Attempts to stop one running native-build process when cancellation is requested.
    /// </summary>
    /// <param name="process">Running native-build process.</param>
    static void TryKillProcess(Process process) {
        if (process == null) {
            return;
        }

        try {
            if (!process.HasExited) {
                process.Kill(entireProcessTree: true);
            }
        } catch {
        }
    }
}
```

- [ ] **Step 4: Run the builder tests to verify the command contract is green**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~PspNativeBuildExecutorTests" -v minimal`

Expected: PASS with all three `PspNativeBuildExecutorTests` green.

- [ ] **Step 5: Commit the default native-build contract change**

```bash
git add builder.tests/PspNativeBuildExecutorTests.cs builder/PspNativeBuildExecutor.cs
git commit -m "Keep PSP runtime startup disabled by default"
```

### Task 2: Thread The Runtime-Startup Switch Through Make And CMake

**Files:**
- Modify: `Makefile`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Update the Makefile to expose the explicit runtime-startup switch**

Replace `Makefile` with:

```makefile
HELENGINE_CORE_CPP_ROOT ?=
HELENGINE_PSP_ISOLATED_BOOT ?= ON
HELENGINE_PSP_ENABLE_RUNTIME_STARTUP ?= OFF

ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
$(error HELENGINE_CORE_CPP_ROOT must point at the generated helengine.core C++ output folder)
endif

BUILD_DIR := build
TARGET_PBP := $(BUILD_DIR)/EBOOT.PBP
CMAKE_ARGS := -DHELENGINE_CORE_CPP_ROOT=$(HELENGINE_CORE_CPP_ROOT) -DHELENGINE_PSP_ISOLATED_BOOT=$(HELENGINE_PSP_ISOLATED_BOOT) -DHELENGINE_PSP_ENABLE_RUNTIME_STARTUP=$(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP)

.PHONY: all clean

all: $(TARGET_PBP)

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && psp-cmake $(CMAKE_ARGS) ..

$(TARGET_PBP): $(BUILD_DIR)/CMakeCache.txt
	$(MAKE) -C $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)
```

- [ ] **Step 2: Update CMake to emit the runtime-startup compile definition without touching generated-core bundling**

Replace `CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.11)

project(helengine_psp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

option(HELENGINE_PSP_ISOLATED_BOOT "Build the PSP player in isolated blank-frame boot mode." ON)
option(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP "Build the PSP player with checkpointed runtime startup enabled." OFF)

if("${HELENGINE_CORE_CPP_ROOT}" STREQUAL "")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT must point at the generated helengine.core C++ output folder.")
endif()

if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/helcpp_config.hpp")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain helcpp_config.hpp. Regenerate helengine.core C++ output first.")
endif()

if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/helengine_core_amalgamated.cpp")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain helengine_core_amalgamated.cpp. Regenerate helengine.core C++ output first.")
endif()

if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_startup_manifest.cpp")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain runtime/runtime_startup_manifest.cpp. Regenerate helengine.core C++ output first.")
endif()

if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_scene_catalog_manifest.cpp")
    message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain runtime/runtime_scene_catalog_manifest.cpp. Regenerate helengine.core C++ output first.")
endif()

set(HELENGINE_PSP_SOURCES
    src/main.cpp
    src/generated-compat/ftruncate_shim.cpp
    src/platform/psp/PspBootHost.cpp
    src/platform/psp/PspAppRootPathResolver.cpp
    src/platform/psp/PspBootTrace.cpp
    src/platform/psp/PspInputBackend.cpp
    src/platform/psp/PspPackagedAssetLoader.cpp
    src/platform/psp/PspRuntimeSceneCatalogFactory.cpp
    src/platform/psp/rendering/PspRenderManager2D.cpp
    src/platform/psp/rendering/PspRuntimeModel.cpp
    src/platform/psp/rendering/PspRenderManager3D.cpp
    "${HELENGINE_CORE_CPP_ROOT}/helengine_core_amalgamated.cpp"
    "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_startup_manifest.cpp"
    "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_scene_catalog_manifest.cpp"
)

if(EXISTS "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_code_module_manifest.cpp")
    list(APPEND HELENGINE_PSP_SOURCES "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_code_module_manifest.cpp")
endif()

add_executable(${PROJECT_NAME} ${HELENGINE_PSP_SOURCES})

if(HELENGINE_PSP_ISOLATED_BOOT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSP_ISOLATED_BOOT=1)
endif()

if(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=1)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/generated-compat
    ${HELENGINE_CORE_CPP_ROOT}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    atomic
    pspdebug
    pspdisplay
    pspge
    pspgu
    pspgum
    pspctrl
)

create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE "Helengine PSP"
    VERSION 01.00
)
```

- [ ] **Step 3: Run the builder tests again to confirm the .NET seam still passes after the build-system edits**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS.

- [ ] **Step 4: Build the isolated baseline to confirm the default native path still compiles**

Run: `docker run --rm -v "${PWD}:/workspace" -v "<generated-core-root>:/generated-core" -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF`

Expected: PASS with `build/EBOOT.PBP` produced.

- [ ] **Step 5: Commit the build-system flag plumbing**

```bash
git add Makefile CMakeLists.txt
git commit -m "Add PSP runtime startup build flag"
```

### Task 3: Replace Monolithic Engine Startup With Checkpointed Boot Stages

**Files:**
- Modify: `src/platform/psp/PspBootHost.hpp`
- Modify: `src/platform/psp/PspBootHost.cpp`

- [ ] **Step 1: Reproduce the current red state for runtime startup with an explicit bring-up build**

Run: `docker run --rm -v "${PWD}:/workspace" -v "<generated-core-root>:/generated-core" -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON`

Then open `build/EBOOT.PBP` in PPSSPP.

Expected: the app still fails opaquely or does not yet identify the first runtime-startup boundary clearly.

- [ ] **Step 2: Declare the checkpointed boot-stage methods and state in the boot-host header**

Replace `src/platform/psp/PspBootHost.hpp` with:

```cpp
#pragma once

#include <string>

class Core;
class CoreInitializationOptions;
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
        /// Initializes the GU state required by the runtime renderer.
        bool InitializeGraphics();

        /// Runs the explicit PSP runtime-startup checkpoints through startup-scene materialization.
        void RunCheckpointedStartup();

        /// Resolves and records the PSP app root used for runtime content access.
        std::string ResolveAppRootPath();

        /// Constructs generated core and PSP platform backends and initializes the runtime.
        void InitializeCore(const std::string& appRootPath);

        /// Loads the packaged startup scene asset from the PSP app content root.
        SceneAsset* LoadStartupSceneAsset(const std::string& appRootPath);

        /// Materializes the startup scene inside generated core.
        void MaterializeStartupScene(SceneAsset* startupScene);

        /// Runs the normal generated-core update and draw loop after startup succeeds.
        void RunMainLoop();

        /// Presents a stable blank frame continuously while generated-core runtime startup is isolated.
        void RunIsolatedFrameLoop();

        /// Records the currently executing boot stage for trace and fatal diagnostics.
        void EnterBootStage(const char* stageName);

        /// Records successful completion of the current boot stage.
        void CompleteBootStage();

        /// Shows one fatal diagnostic message on the PSP screen and keeps the app alive for inspection.
        void ShowFatalErrorAndHalt(const std::string& message);

        /// Begins one PSP frame and clears the color and depth buffers.
        void BeginFrame();

        /// Presents the current PSP frame to the display.
        void PresentFrame();

        /// Stores the aligned GU display-list buffer used for command submission.
        unsigned int* DisplayList;

        /// Stores the executable path reported by the PSP entrypoint.
        std::string ExecutablePath;

        /// Stores the currently active PSP boot stage for diagnostics.
        const char* CurrentBootStage;

        /// Stores the resolved PSP app root used by runtime startup.
        std::string AppRootPath;

        /// Stores the generated runtime core instance.
        ::Core* EngineCore;

        /// Stores the generated runtime initialization options owned by the core.
        ::CoreInitializationOptions* EngineOptions;

        /// Stores the active PSP 3D render manager.
        ::RenderManager3D* EngineRenderManager3D;

        /// Stores the active PSP 2D render manager.
        ::RenderManager2D* EngineRenderManager2D;

        /// Stores the PSP input backend used by generated core.
        ::IInputBackend* EngineInputBackend;
    };
}
```

- [ ] **Step 3: Implement staged startup, stage logging, and stage-aware fatal diagnostics**

Replace `src/platform/psp/PspBootHost.cpp` with:

```cpp
#include "platform/psp/PspBootHost.hpp"

#include <cstdio>
#include <exception>
#include <string>

#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspuser.h>

#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "RenderManager2D.hpp"
#include "RenderManager3D.hpp"
#include "SceneAsset.hpp"
#include "platform/psp/PspAppRootPathResolver.hpp"
#include "platform/psp/PspBootTrace.hpp"
#include "platform/psp/PspInputBackend.hpp"
#include "platform/psp/PspPackagedAssetLoader.hpp"
#include "platform/psp/rendering/PspRenderManager2D.hpp"
#include "platform/psp/rendering/PspRenderManager3D.hpp"

PSP_MODULE_INFO("helengine_psp", 0, 1, 0);
PSP_HEAP_SIZE_KB(16 * 1024);
PSP_MAIN_THREAD_STACK_SIZE_KB(512);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU | THREAD_ATTR_USER);

namespace helengine::psp {
    namespace {
        constexpr int BufferWidth = 512;
        constexpr int BufferHeight = 272;
        constexpr int ScreenWidth = 480;
        constexpr int ScreenHeight = BufferHeight;
        constexpr unsigned int DefaultClearColor = 0xFF101010;
        constexpr const char* GraphicsInitializationStageName = "GraphicsInitialization";
        constexpr const char* IsolatedFrameLoopStageName = "IsolatedFrameLoop";
        constexpr const char* RuntimeAppRootStageName = "RuntimeAppRootResolution";
        constexpr const char* RuntimeCoreInitializationStageName = "RuntimeCoreInitialization";
        constexpr const char* RuntimeStartupSceneAssetLoadStageName = "RuntimeStartupSceneAssetLoad";
        constexpr const char* RuntimeStartupSceneMaterializationStageName = "RuntimeStartupSceneMaterialization";
        constexpr const char* RuntimeMainLoopStageName = "RuntimeMainLoop";

        alignas(64) unsigned int DisplayListStorage[0x20000 / sizeof(unsigned int)];
    }

    /// Creates the PSP boot host with no initialized graphics or engine state.
    PspBootHost::PspBootHost(const std::string& executablePath)
        : DisplayList(DisplayListStorage),
          ExecutablePath(executablePath),
          CurrentBootStage("ProcessEntry"),
          AppRootPath(),
          EngineCore(nullptr),
          EngineOptions(nullptr),
          EngineRenderManager3D(nullptr),
          EngineRenderManager2D(nullptr),
          EngineInputBackend(nullptr) {
    }

    /// Initializes the PSP runtime and presents frames until shutdown.
    int PspBootHost::Run() {
        try {
            PspBootTrace::WriteLine("Run begin");
            EnterBootStage(GraphicsInitializationStageName);
            if (!InitializeGraphics()) {
                PspBootTrace::WriteLine("InitializeGraphics returned false");
                return 1;
            }
            CompleteBootStage();

#if defined(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP) && HELENGINE_PSP_ENABLE_RUNTIME_STARTUP
            PspBootTrace::WriteLine("Checkpointed runtime startup enabled.");
            RunCheckpointedStartup();
            return 0;
#elif defined(HELENGINE_PSP_ISOLATED_BOOT) && HELENGINE_PSP_ISOLATED_BOOT
            PspBootTrace::WriteLine("Isolated boot enabled. Skipping generated-core runtime startup.");
            RunIsolatedFrameLoop();
            return 0;
#else
            PspBootTrace::WriteLine("No explicit runtime-startup mode was selected. Falling back to isolated boot.");
            RunIsolatedFrameLoop();
            return 0;
#endif
        } catch (const std::exception& exception) {
            std::printf("[helengine-psp] fatal exception: %s\n", exception.what());
            std::fflush(stdout);
            ShowFatalErrorAndHalt(exception.what());
            return 1;
        } catch (...) {
            std::printf("[helengine-psp] fatal unknown exception\n");
            std::fflush(stdout);
            ShowFatalErrorAndHalt("Unknown fatal exception.");
            return 1;
        }
    }

    /// Initializes the GU state required by the runtime renderer.
    bool PspBootHost::InitializeGraphics() {
        PspBootTrace::WriteLine("InitializeGraphics start");
        sceGuInit();
        sceGuStart(GU_DIRECT, DisplayList);
        sceGuDrawBuffer(GU_PSM_8888, reinterpret_cast<void*>(0), BufferWidth);
        sceGuDispBuffer(ScreenWidth, ScreenHeight, reinterpret_cast<void*>(0x88000), BufferWidth);
        sceGuDepthBuffer(reinterpret_cast<void*>(0x110000), BufferWidth);
        sceGuOffset(2048 - (ScreenWidth / 2), 2048 - (ScreenHeight / 2));
        sceGuViewport(2048, 2048, ScreenWidth, ScreenHeight);
        sceGuDepthRange(65535, 0);
        sceGuDepthFunc(GU_GEQUAL);
        sceGuEnable(GU_DEPTH_TEST);
        sceGuEnable(GU_SCISSOR_TEST);
        sceGuScissor(0, 0, ScreenWidth, ScreenHeight);
        sceGuDisable(GU_CULL_FACE);
        sceGuDisable(GU_TEXTURE_2D);
        sceGuShadeModel(GU_SMOOTH);
        sceGuFinish();
        sceGuSync(0, 0);
        sceGuDisplay(GU_TRUE);
        PspBootTrace::WriteLine("InitializeGraphics end");
        return true;
    }

    /// Runs the explicit PSP runtime-startup checkpoints through startup-scene materialization.
    void PspBootHost::RunCheckpointedStartup() {
        std::string appRootPath = ResolveAppRootPath();
        InitializeCore(appRootPath);
        SceneAsset* startupScene = LoadStartupSceneAsset(appRootPath);
        MaterializeStartupScene(startupScene);
        RunMainLoop();
    }

    /// Resolves and records the PSP app root used for runtime content access.
    std::string PspBootHost::ResolveAppRootPath() {
        EnterBootStage(RuntimeAppRootStageName);
        PspAppRootPathResolver appRootPathResolver;
        AppRootPath = appRootPathResolver.ResolveAppRootPath(ExecutablePath);
        PspBootTrace::SetAppRootPath(AppRootPath);
        PspBootTrace::WriteLine("Resolved appRootPath=" + AppRootPath);
        CompleteBootStage();
        return AppRootPath;
    }

    /// Constructs generated core and PSP platform backends and initializes the runtime.
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

        EngineRenderManager3D = new rendering::PspRenderManager3D();
        EngineRenderManager2D = new rendering::PspRenderManager2D();
        EngineInputBackend = new PspInputBackend();

        EngineRenderManager3D->AddWindow(0, ScreenWidth, ScreenHeight);
        EngineCore->Initialize(
            EngineRenderManager3D,
            EngineRenderManager2D,
            EngineInputBackend,
            EngineOptions);

        CompleteBootStage();
    }

    /// Loads the packaged startup scene asset from the PSP app content root.
    SceneAsset* PspBootHost::LoadStartupSceneAsset(const std::string& appRootPath) {
        EnterBootStage(RuntimeStartupSceneAssetLoadStageName);
        PspPackagedAssetLoader packagedAssetLoader(appRootPath);
        SceneAsset* startupScene = packagedAssetLoader.LoadStartupScene();
        PspBootTrace::WriteLine("Startup scene asset loaded.");
        CompleteBootStage();
        return startupScene;
    }

    /// Materializes the startup scene inside generated core.
    void PspBootHost::MaterializeStartupScene(SceneAsset* startupScene) {
        EnterBootStage(RuntimeStartupSceneMaterializationStageName);
        EngineCore->get_SceneLoadService()->Load(startupScene);
        PspBootTrace::WriteLine("Startup scene instantiated.");
        CompleteBootStage();
    }

    /// Runs the normal generated-core update and draw loop after startup succeeds.
    void PspBootHost::RunMainLoop() {
        EnterBootStage(RuntimeMainLoopStageName);
        CompleteBootStage();

        while (true) {
            EngineCore->Update();
            BeginFrame();
            EngineCore->Draw();
            PresentFrame();
        }
    }

    /// Presents a stable blank frame continuously while generated-core runtime startup is isolated.
    void PspBootHost::RunIsolatedFrameLoop() {
        EnterBootStage(IsolatedFrameLoopStageName);
        CompleteBootStage();

        while (true) {
            BeginFrame();
            PresentFrame();
        }
    }

    /// Records the currently executing boot stage for trace and fatal diagnostics.
    void PspBootHost::EnterBootStage(const char* stageName) {
        CurrentBootStage = stageName;
        PspBootTrace::WriteLine(std::string("Stage begin ") + CurrentBootStage);
    }

    /// Records successful completion of the current boot stage.
    void PspBootHost::CompleteBootStage() {
        PspBootTrace::WriteLine(std::string("Stage complete ") + CurrentBootStage);
    }

    /// Shows one fatal diagnostic message on the PSP screen and keeps the app alive for inspection.
    void PspBootHost::ShowFatalErrorAndHalt(const std::string& message) {
        PspBootTrace::WriteLine(std::string("FatalError stage=") + CurrentBootStage + " message=" + message);
        pspDebugScreenInit();
        pspDebugScreenSetXY(0, 0);
        pspDebugScreenPrintf("helengine-psp fatal error\n\n");
        pspDebugScreenPrintf("stage: %s\n\n", CurrentBootStage);
        pspDebugScreenPrintf("%s\n", message.c_str());
        pspDebugScreenPrintf("\nThe app is halted for diagnostics.\n");
        while (true) {
            sceDisplayWaitVblankStart();
            sceKernelDelayThread(100000);
        }
    }

    /// Begins one PSP frame and clears the color and depth buffers.
    void PspBootHost::BeginFrame() {
        sceGuStart(GU_DIRECT, DisplayList);
        sceGuClearColor(DefaultClearColor);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    }

    /// Presents the current PSP frame to the display.
    void PspBootHost::PresentFrame() {
        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }
}
```

- [ ] **Step 4: Build the isolated baseline again to verify the refactor did not regress the known-good path**

Run: `docker run --rm -v "${PWD}:/workspace" -v "<generated-core-root>:/generated-core" -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF`

Expected: PASS with `build/EBOOT.PBP` rebuilt successfully.

- [ ] **Step 5: Build the checkpointed runtime-startup variant**

Run: `docker run --rm -v "${PWD}:/workspace" -v "<generated-core-root>:/generated-core" -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON`

Expected: PASS with `build/EBOOT.PBP` rebuilt successfully for runtime bring-up.

- [ ] **Step 6: Launch the checkpointed runtime-startup build in PPSSPP and capture the first real fault boundary**

Open `build/EBOOT.PBP` in PPSSPP.

Expected:

- the app either reaches startup-scene materialization and stays alive, or
- the app halts on-screen with a `stage:` line that names the failing checkpoint

- [ ] **Step 7: Commit the checkpointed boot-host implementation**

```bash
git add src/platform/psp/PspBootHost.hpp src/platform/psp/PspBootHost.cpp
git commit -m "Add PSP runtime startup checkpoints"
```

### Task 4: Document The Runtime Bring-Up Modes And Re-Verify

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update the README to describe both the isolated baseline and the checkpointed runtime-startup build**

Replace `README.md` with:

````md
# Helengine PSP Host

This repository contains the native PSP host scaffold for Helengine.

## Current milestone

- Docker-only build using the official PSPDEV SDK image
- Standard PSP homebrew packaging ending in `EBOOT.PBP`
- Generated Helengine runtime sources still bundled into the PSP executable
- Isolated blank-frame boot remains the default verification baseline
- Checkpointed runtime startup can be enabled explicitly to diagnose PSP startup through startup-scene load

## Build

Default isolated baseline:

```bash
docker build -t helengine-psp .
docker run --rm -v "$PWD":/workspace -v "$HELENGINE_CORE_CPP_ROOT":/generated-core -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF
```

Checkpointed runtime-startup bring-up:

```bash
docker build -t helengine-psp .
docker run --rm -v "$PWD":/workspace -v "$HELENGINE_CORE_CPP_ROOT":/generated-core -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

If Docker Desktop's credential helper blocks anonymous pulls on this machine, use:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-psp .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -v "$HELENGINE_CORE_CPP_ROOT":/generated-core -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF
```

The build emits `build/EBOOT.PBP`.

## Generated core seam

The native PSP build requires `HELENGINE_CORE_CPP_ROOT` to point at the generated core output produced by the editor build graph. The generated core root must contain:

- `helcpp_config.hpp`
- `helengine_core_amalgamated.cpp`
- `runtime/runtime_startup_manifest.cpp`
- `runtime/runtime_scene_catalog_manifest.cpp`

When runtime code modules exist, the native build also picks up `runtime/runtime_code_module_manifest.cpp`.

## End-to-end PSP build

The real PSP build is editor-driven. The editor regenerates generated-core C++ for platform `psp`, cooks the selected scene and assets, and then invokes the PSP builder assembly in this repository.

Expected staged output:

- `output/psp/PSP/GAME/HELENGINE/EBOOT.PBP`
- `output/psp/PSP/GAME/HELENGINE/cooked/scenes/rendering/cube_test.hasset`

## Boot check

Load `build/EBOOT.PBP` in PPSSPP.

- With `HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF`, the expected result is a stable blank frame with no immediate crash or reset loop.
- With `HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON`, the expected result is either successful startup-scene load or an on-screen fatal diagnostic that names the failing checkpoint.
````

- [ ] **Step 2: Re-run the builder tests after the documentation-aligned code changes**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS.

- [ ] **Step 3: Re-run the default isolated native build**

Run: `docker run --rm -v "${PWD}:/workspace" -v "<generated-core-root>:/generated-core" -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF`

Expected: PASS with the isolated baseline still producing `build/EBOOT.PBP`.

- [ ] **Step 4: Re-run the checkpointed runtime-startup native build**

Run: `docker run --rm -v "${PWD}:/workspace" -v "<generated-core-root>:/generated-core" -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON`

Expected: PASS with the runtime-startup bring-up build still producing `build/EBOOT.PBP`.

- [ ] **Step 5: Perform the final PPSSPP runtime-startup verification**

Open `build/EBOOT.PBP` in PPSSPP using the runtime-startup build.

Expected:

- the app reaches startup-scene load and proceeds, or
- the app halts with a checkpoint-specific `stage:` diagnostic on screen

- [ ] **Step 6: Commit the documentation and verification update**

```bash
git add README.md
git commit -m "Document PSP runtime startup bring-up"
```
