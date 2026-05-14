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
        } catch (const Exception* exception) {
            const char* message = exception != nullptr ? exception->what() : "Unknown managed runtime exception.";
            std::printf("[helengine-psp] fatal runtime exception: %s\n", message);
            std::fflush(stdout);
            ShowFatalErrorAndHalt(message);
            delete exception;
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

    /// Builds the runtime platform metadata embedded into the PSP generated startup manifest.
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
