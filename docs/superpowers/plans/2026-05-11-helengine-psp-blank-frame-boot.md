# Helengine PSP Blank-Frame Boot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the PSP player boot and stay alive in PPSSPP on a stable blank frame while still compiling and bundling the generated Helengine runtime sources and manifests.

**Architecture:** Keep the existing editor and builder contract intact, including generated-core compilation and `EBOOT.PBP` packaging, but add one explicit isolated-boot build flag that bypasses runtime execution after GU initialization. The builder/native build path will pass that flag through Docker, Make, and CMake, and `PspBootHost` will use it to run a graphics-only frame loop instead of engine startup.

**Tech Stack:** .NET 9, xUnit, Docker, PSPDEV/PSPSDK, GNU Make, CMake, C++17, PSP GU/GUM, PPSSPP

---

## File Structure

- Modify: `builder.tests/PspNativeBuildExecutorTests.cs`
  Purpose: lock the native-build command contract so generated-core mounting remains intact and isolated boot is enabled explicitly.
- Modify: `builder/PspNativeBuildExecutor.cs`
  Purpose: append the isolated-boot make variable without changing the existing generated-core mount contract.
- Modify: `Makefile`
  Purpose: default isolated boot to `ON` and forward it into the CMake configure step.
- Modify: `CMakeLists.txt`
  Purpose: expose the isolated-boot option to the PSP player compile and define the C++ preprocessor symbol.
- Modify: `src/platform/psp/PspBootHost.hpp`
  Purpose: declare the isolated blank-frame loop entrypoint.
- Modify: `src/platform/psp/PspBootHost.cpp`
  Purpose: bypass generated-core initialization when isolated boot is enabled and stay in a GU/VBlank frame loop.
- Modify: `README.md`
  Purpose: document the isolated-boot milestone and the expected PPSSPP blank-frame result.

### Task 1: Wire The Isolated-Boot Build Contract

**Files:**
- Modify: `builder.tests/PspNativeBuildExecutorTests.cs`
- Modify: `builder/PspNativeBuildExecutor.cs`
- Modify: `Makefile`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing native-build executor regression test**

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
        Assert.Contains("/repo:/workspace", arguments);
        Assert.Contains("/generated-core:/generated-core", arguments);
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
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~CreateBuildArguments_whenWorkspaceIsProvided_enables_isolated_boot" -v minimal`

Expected: FAIL because `PspNativeBuildExecutor.CreateBuildArguments(...)` does not yet include `HELENGINE_PSP_ISOLATED_BOOT=ON`.

- [ ] **Step 3: Update the native-build executor to pass the isolated-boot flag**

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
            "HELENGINE_PSP_ISOLATED_BOOT=ON"
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

- [ ] **Step 4: Run the targeted test to verify it passes**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj --filter "FullyQualifiedName~PspNativeBuildExecutorTests" -v minimal`

Expected: PASS with both `PspNativeBuildExecutorTests` green.

- [ ] **Step 5: Forward the isolated-boot flag through Make and CMake**

```makefile
HELENGINE_CORE_CPP_ROOT ?=
HELENGINE_PSP_ISOLATED_BOOT ?= ON

ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
$(error HELENGINE_CORE_CPP_ROOT must point at the generated helengine.core C++ output folder)
endif

BUILD_DIR := build
TARGET_PBP := $(BUILD_DIR)/EBOOT.PBP
CMAKE_ARGS := -DHELENGINE_CORE_CPP_ROOT=$(HELENGINE_CORE_CPP_ROOT) -DHELENGINE_PSP_ISOLATED_BOOT=$(HELENGINE_PSP_ISOLATED_BOOT)

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

```cmake
cmake_minimum_required(VERSION 3.11)

project(helengine_psp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

option(HELENGINE_PSP_ISOLATED_BOOT "Build the PSP player in isolated blank-frame boot mode." ON)

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

- [ ] **Step 6: Run the builder tests again after the build-contract changes**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS with the PSP builder and native-build executor tests green.

- [ ] **Step 7: Commit the isolated-build-contract changes**

```bash
git add builder.tests/PspNativeBuildExecutorTests.cs builder/PspNativeBuildExecutor.cs Makefile CMakeLists.txt
git commit -m "Add PSP isolated boot build flag"
```

### Task 2: Isolate The PSP Boot Host To A Blank-Frame Loop

**Files:**
- Modify: `src/platform/psp/PspBootHost.hpp`
- Modify: `src/platform/psp/PspBootHost.cpp`

- [ ] **Step 1: Reproduce the current red state in PPSSPP**

Open `build/EBOOT.PBP` in PPSSPP using the same local launch flow you used for the previous crash repro.

Expected: the player crashes or exits before reaching a stable frame, confirming the current runtime path is still red for this milestone.

- [ ] **Step 2: Add the isolated frame-loop entrypoint to the boot-host header**

```cpp
#pragma once

#include <string>

class Core;
class CoreInitializationOptions;
class RenderManager2D;
class RenderManager3D;
class IInputBackend;

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

        /// Initializes generated core, render managers, input, and the packaged startup scene.
        bool InitializeEngine();

        /// Presents a stable blank frame continuously while generated-core runtime startup is isolated.
        void RunIsolatedFrameLoop();

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

- [ ] **Step 3: Implement the isolated boot path in the PSP boot host**

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

        alignas(64) unsigned int DisplayListStorage[0x20000 / sizeof(unsigned int)];
    }

    /// Creates the PSP boot host with no initialized graphics or engine state.
    PspBootHost::PspBootHost(const std::string& executablePath)
        : DisplayList(DisplayListStorage),
          ExecutablePath(executablePath),
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
            if (!InitializeGraphics()) {
                PspBootTrace::WriteLine("InitializeGraphics returned false");
                return 1;
            }
            PspBootTrace::WriteLine("InitializeGraphics complete");

#if defined(HELENGINE_PSP_ISOLATED_BOOT) && HELENGINE_PSP_ISOLATED_BOOT
            PspBootTrace::WriteLine("Isolated boot enabled. Skipping generated-core runtime startup.");
            RunIsolatedFrameLoop();
            return 0;
#else
            if (!InitializeEngine()) {
                PspBootTrace::WriteLine("InitializeEngine returned false");
                return 1;
            }
            PspBootTrace::WriteLine("InitializeEngine complete");

            while (true) {
                EngineCore->Update();
                BeginFrame();
                EngineCore->Draw();
                PresentFrame();
            }
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

    /// Presents a stable blank frame continuously while generated-core runtime startup is isolated.
    void PspBootHost::RunIsolatedFrameLoop() {
        PspBootTrace::WriteLine("RunIsolatedFrameLoop begin");
        while (true) {
            BeginFrame();
            PresentFrame();
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

    /// Initializes generated core, render managers, input, and the packaged startup scene.
    bool PspBootHost::InitializeEngine() {
        PspAppRootPathResolver appRootPathResolver;
        std::string appRootPath = appRootPathResolver.ResolveAppRootPath(ExecutablePath);
        PspBootTrace::SetAppRootPath(appRootPath);
        PspBootTrace::WriteLine("InitializeEngine appRootPath=" + appRootPath);

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

        PspPackagedAssetLoader packagedAssetLoader(appRootPath);
        PspBootTrace::WriteLine("Loading startup scene.");
        SceneAsset* startupScene = packagedAssetLoader.LoadStartupScene();
        PspBootTrace::WriteLine("Startup scene loaded.");
        EngineCore->get_SceneLoadService()->Load(startupScene);
        PspBootTrace::WriteLine("Startup scene instantiated.");
        return true;
    }

    /// Shows one fatal diagnostic message on the PSP screen and keeps the app alive for inspection.
    void PspBootHost::ShowFatalErrorAndHalt(const std::string& message) {
        PspBootTrace::WriteLine("FatalError " + message);
        pspDebugScreenInit();
        pspDebugScreenSetXY(0, 0);
        pspDebugScreenPrintf("helengine-psp fatal error\n\n");
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

- [ ] **Step 4: Build and verify the native player compiles with generated core still bundled**

Run: `docker run --rm -v "${PWD}:/workspace" -v "<generated-core-root>:/generated-core" -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON`

Expected: PASS with `build/EBOOT.PBP` produced and no change to the generated-core input contract.

- [ ] **Step 5: Re-run PPSSPP and verify the green state**

Open `build/EBOOT.PBP` in PPSSPP using the same local launch flow as the red repro.

Expected:

- PPSSPP stays alive
- no immediate crash or reset loop occurs
- the player presents a stable blank frame

- [ ] **Step 6: Commit the isolated PSP boot-host change**

```bash
git add src/platform/psp/PspBootHost.hpp src/platform/psp/PspBootHost.cpp
git commit -m "Isolate PSP boot to blank frame loop"
```

### Task 3: Document The Isolated-Boot Milestone And Re-Run Full Verification

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update the README to describe the isolated blank-frame milestone**

````md
# Helengine PSP Host

This repository contains the native PSP host scaffold for Helengine.

## Current milestone

- Docker-only build using the official PSPDEV SDK image
- Standard PSP homebrew packaging ending in `EBOOT.PBP`
- Generated Helengine runtime sources still bundled into the PSP executable
- Isolated blank-frame boot in PPSSPP while runtime startup is temporarily disabled

## Build

```bash
docker build -t helengine-psp .
docker run --rm -v "$PWD":/workspace -v "$HELENGINE_CORE_CPP_ROOT":/generated-core -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON
```

If Docker Desktop's credential helper blocks anonymous pulls on this machine, use:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-psp .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -v "$HELENGINE_CORE_CPP_ROOT":/generated-core -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON
```

The build emits `build/EBOOT.PBP`.

## Generated core seam

The native PSP build now requires `HELENGINE_CORE_CPP_ROOT` to point at the generated core output produced by the editor build graph. The generated core root must contain:

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

Load `build/EBOOT.PBP` in PPSSPP. The expected result for this milestone is a stable blank frame with no immediate crash or reset loop.
````

- [ ] **Step 2: Run the full automated verification again**

Run: `dotnet test builder.tests/helengine.psp.builder.tests.csproj -v minimal`

Expected: PASS.

- [ ] **Step 3: Re-run the native Docker build after the README-aligned milestone changes**

Run: `docker run --rm -v "${PWD}:/workspace" -v "<generated-core-root>:/generated-core" -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON`

Expected: PASS with `build/EBOOT.PBP` rebuilt successfully.

- [ ] **Step 4: Perform the final PPSSPP boot verification**

Open `build/EBOOT.PBP` in PPSSPP and confirm:

- the app stays alive
- the screen remains in the expected stable blank-frame state
- no runtime-scene or packaged-asset work is required to keep the player running

- [ ] **Step 5: Commit the README and verification milestone update**

```bash
git add README.md
git commit -m "Document PSP isolated blank-frame milestone"
```
