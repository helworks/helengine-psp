#include "platform/psp/PspBootHost.hpp"

#include <cstdio>
#include <exception>
#include <new>
#include <string>

#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspuser.h>

#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "LoadedSceneRecord.hpp"
#include "PlatformInfo.hpp"
#include "RenderManager2D.hpp"
#include "RenderManager3D.hpp"
#include "SceneManager.hpp"
#include "SceneAsset.hpp"
#include "platform/psp/PspAppRootPathResolver.hpp"
#include "platform/psp/PspBootTrace.hpp"
#include "platform/psp/PspInputBackend.hpp"
#include "platform/psp/PspPackagedAssetLoader.hpp"
#include "platform/psp/PspRuntimeSceneCatalogFactory.hpp"
#include "platform/psp/rendering/PspRenderManager2D.hpp"
#include "platform/psp/rendering/PspRenderManager3D.hpp"
#include "runtime/native_exceptions.hpp"

#if defined(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP) && HELENGINE_PSP_ENABLE_RUNTIME_STARTUP
#include "runtime/runtime_scene_catalog_manifest.hpp"
#include "runtime/runtime_startup_manifest.hpp"
#endif

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
          EngineInputBackend(nullptr),
          LastTracedLoadedSceneCount(-1),
          LastTracedPrimarySceneId(),
          WasReturnButtonDownLastFrame(false),
          ReturnTransitionTraceFramesRemaining(0) {
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
#if defined(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP) && HELENGINE_PSP_ENABLE_RUNTIME_STARTUP
        std::string appRootPath = ResolveAppRootPath();
        InitializeCore(appRootPath);
        LoadStartupScene();
        RunMainLoop();
#else
        throw std::runtime_error("Checkpointed runtime startup was not compiled into this PSP build.");
#endif
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
#if defined(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP) && HELENGINE_PSP_ENABLE_RUNTIME_STARTUP
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
#else
        throw std::runtime_error("Runtime platform info is only available when PSP runtime startup is enabled.");
#endif
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
        PspRuntimeSceneCatalogFactory runtimeSceneCatalogFactory;
        EngineOptions->set_SceneCatalog(runtimeSceneCatalogFactory.Build());

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

    /// Loads the configured startup scene through the runtime scene manager so scene lifetime stays tracked from frame one.
    void PspBootHost::LoadStartupScene() {
#if defined(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP) && HELENGINE_PSP_ENABLE_RUNTIME_STARTUP
        EnterBootStage(RuntimeStartupSceneAssetLoadStageName);
        const char* startupSceneRelativePath = he_get_runtime_startup_scene_relative_path();
        if (startupSceneRelativePath == nullptr || startupSceneRelativePath[0] == '\0') {
            throw std::runtime_error("PSP runtime startup manifest did not define a startup scene.");
        }

        std::size_t runtimeSceneCount = 0;
        const HERuntimeSceneCatalogEntry* runtimeSceneEntries = he_runtime_scene_catalog_entries(&runtimeSceneCount);
        if (runtimeSceneEntries == nullptr || runtimeSceneCount == 0) {
            throw std::runtime_error("PSP runtime scene catalog manifest did not contain any entries.");
        }

        std::string startupSceneId;
        for (std::size_t index = 0; index < runtimeSceneCount; index++) {
            const HERuntimeSceneCatalogEntry& runtimeSceneEntry = runtimeSceneEntries[index];
            if (runtimeSceneEntry.CookedRelativePath != nullptr && std::string(runtimeSceneEntry.CookedRelativePath) == startupSceneRelativePath) {
                startupSceneId = runtimeSceneEntry.SceneId;
                break;
            }
        }

        if (startupSceneId.empty()) {
            throw std::runtime_error("PSP runtime startup scene path was not found in the runtime scene catalog manifest.");
        }

        PspBootTrace::WriteLine(std::string("LoadStartupScene id=") + startupSceneId + " path=" + startupSceneRelativePath);
        CompleteBootStage();
        EnterBootStage(RuntimeStartupSceneMaterializationStageName);
        if (EngineCore->get_SceneManager() == nullptr) {
            throw std::runtime_error("PSP runtime scene manager was not initialized before startup scene loading.");
        }

        EngineCore->get_SceneManager()->LoadScene(startupSceneId, SceneLoadMode::Single);
        PspBootTrace::WriteLine("Startup scene instantiated.");
        CompleteBootStage();
#else
        throw std::runtime_error("Startup scene loading is only available when PSP runtime startup is enabled.");
#endif
    }

    /// Returns the current primary loaded-scene id, or an empty string when no scene is active.
    std::string PspBootHost::GetPrimarySceneId() const {
        if (EngineCore == nullptr || EngineCore->get_SceneManager() == nullptr) {
            return std::string();
        }

        List<LoadedSceneRecord*>* loadedScenes = EngineCore->get_SceneManager()->get_LoadedScenes();
        if (loadedScenes == nullptr || loadedScenes->get_Count() <= 0 || (*loadedScenes)[0] == nullptr) {
            return std::string();
        }

        return (*loadedScenes)[0]->get_SceneId();
    }

    /// Returns the current runtime loaded-scene count.
    int32_t PspBootHost::GetLoadedSceneCount() const {
        if (EngineCore == nullptr || EngineCore->get_SceneManager() == nullptr) {
            return 0;
        }

        List<LoadedSceneRecord*>* loadedScenes = EngineCore->get_SceneManager()->get_LoadedScenes();
        if (loadedScenes == nullptr) {
            return 0;
        }

        return loadedScenes->get_Count();
    }

    /// Returns whether the PSP return bind is currently held on raw pad input.
    bool PspBootHost::IsReturnButtonDown() const {
        sceCtrlSetSamplingCycle(0);
        sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

        SceCtrlData padData {};
        sceCtrlPeekBufferPositive(&padData, 1);
        return (padData.Buttons & PSP_CTRL_CIRCLE) != 0
            || (padData.Buttons & PSP_CTRL_TRIANGLE) != 0
            || (padData.Buttons & PSP_CTRL_SELECT) != 0;
    }

    /// Emits one focused runtime transition trace line with scene, memory, and input state.
    void PspBootHost::TraceRuntimeTransitionState(const char* phaseName, const std::string& primarySceneId, int32_t loadedSceneCount, int32_t freeMemoryBytes, bool returnButtonDown) const {
        PspBootTrace::WriteLine(
            std::string("TransitionTrace phase=") + phaseName
            + " primary=" + (primarySceneId.empty() ? std::string("<none>") : primarySceneId)
            + " count=" + std::to_string(loadedSceneCount)
            + " freeMem=" + std::to_string(freeMemoryBytes)
            + " returnDown=" + (returnButtonDown ? std::string("true") : std::string("false")));
    }

    /// Emits one focused managed-runtime transition snapshot captured inside generated-core scene loading code.
    void PspBootHost::TraceManagedTransitionState(const char* phaseName) const {
        if (EngineCore == nullptr) {
            PspBootTrace::WriteLine(std::string("ManagedTransitionTrace phase=") + phaseName + " core=null");
            return;
        }

        SceneManager* sceneManager = EngineCore->get_SceneManager();
        if (sceneManager == nullptr) {
            PspBootTrace::WriteLine(
                std::string("ManagedTransitionTrace phase=") + phaseName
                + " coreStage=" + EngineCore->get_LastSceneTransitionStage()
                + " sceneManager=null");
            return;
        }

        PspBootTrace::WriteLine(
            std::string("ManagedTransitionTrace phase=") + phaseName
            + " coreStage=" + EngineCore->get_LastSceneTransitionStage()
            + " sceneStage=" + sceneManager->get_LastTraceStage()
            + " sceneId=" + sceneManager->get_LastTraceSceneId()
            + " loadedSceneCount=" + std::to_string(sceneManager->get_LastTraceLoadedSceneCount())
            + " pendingCount=" + std::to_string(sceneManager->get_LastTracePendingOperationCount())
            + " loadStage=" + EngineCore->get_SceneLoadService()->get_LastTraceStage()
            + " loadRootIndex=" + std::to_string(EngineCore->get_SceneLoadService()->get_LastTraceRootEntityIndex())
            + " loadDepth=" + std::to_string(EngineCore->get_SceneLoadService()->get_LastTraceEntityDepth())
            + " loadComponentType=" + EngineCore->get_SceneLoadService()->get_LastTraceComponentTypeId()
            + " textLoadStage=" + EngineCore->get_SceneLoadService()->get_LastTextLoadStage()
            + " textFontPath=" + EngineCore->get_SceneLoadService()->get_LastTextFontRelativePath()
            + " fontDeserializeStage=" + EngineCore->get_SceneLoadService()->get_LastFontDeserializeStage());
    }

    /// Runs the normal generated-core update and draw loop after startup succeeds.
    void PspBootHost::RunMainLoop() {
        EnterBootStage(RuntimeMainLoopStageName);
        CompleteBootStage();

        while (true) {
            const std::string preUpdatePrimarySceneId = GetPrimarySceneId();
            const int32_t preUpdateLoadedSceneCount = GetLoadedSceneCount();
            const int32_t preUpdateFreeMemoryBytes = sceKernelTotalFreeMemSize();
            const bool returnButtonDown = IsReturnButtonDown();
            const bool returnButtonPressedThisFrame = returnButtonDown && !WasReturnButtonDownLastFrame;
            WasReturnButtonDownLastFrame = returnButtonDown;

            if (returnButtonPressedThisFrame && !preUpdatePrimarySceneId.empty() && preUpdatePrimarySceneId != "DemoDiscMainMenu") {
                ReturnTransitionTraceFramesRemaining = 90;
                TraceRuntimeTransitionState("ReturnBindPressed", preUpdatePrimarySceneId, preUpdateLoadedSceneCount, preUpdateFreeMemoryBytes, returnButtonDown);
            }

            if (ReturnTransitionTraceFramesRemaining > 0) {
                TraceRuntimeTransitionState("BeforeUpdate", preUpdatePrimarySceneId, preUpdateLoadedSceneCount, preUpdateFreeMemoryBytes, returnButtonDown);
            }

            try {
                EngineCore->Update();
            } catch (const std::bad_alloc&) {
                TraceRuntimeTransitionState("UpdateBadAlloc", preUpdatePrimarySceneId, preUpdateLoadedSceneCount, preUpdateFreeMemoryBytes, returnButtonDown);
                TraceManagedTransitionState("UpdateBadAlloc");
                throw;
            }

            if (ReturnTransitionTraceFramesRemaining > 0) {
                TraceRuntimeTransitionState(
                    "AfterUpdate",
                    GetPrimarySceneId(),
                    GetLoadedSceneCount(),
                    sceKernelTotalFreeMemSize(),
                    IsReturnButtonDown());
            }

            SceneManager* sceneManager = EngineCore->get_SceneManager();
            if (sceneManager != nullptr) {
                List<LoadedSceneRecord*>* loadedScenes = sceneManager->get_LoadedScenes();
                const int32_t loadedSceneCount = loadedScenes != nullptr ? loadedScenes->get_Count() : 0;
                std::string primarySceneId;
                if (loadedScenes != nullptr && loadedSceneCount > 0 && (*loadedScenes)[0] != nullptr) {
                    primarySceneId = (*loadedScenes)[0]->get_SceneId();
                }
                const int32_t entityCount = EngineCore->get_ObjectManager() != nullptr && EngineCore->get_ObjectManager()->get_Entities() != nullptr
                    ? EngineCore->get_ObjectManager()->get_Entities()->get_Count()
                    : 0;
                const int32_t cameraCount = EngineCore->get_ObjectManager() != nullptr && EngineCore->get_ObjectManager()->get_Cameras() != nullptr
                    ? EngineCore->get_ObjectManager()->get_Cameras()->get_Count()
                    : 0;
                const int32_t freeMemoryBytes = sceKernelTotalFreeMemSize();

                if (loadedSceneCount != LastTracedLoadedSceneCount || primarySceneId != LastTracedPrimarySceneId) {
                    PspBootTrace::WriteLine(
                        std::string("SceneState count=") + std::to_string(loadedSceneCount)
                        + " primary=" + (primarySceneId.empty() ? std::string("<none>") : primarySceneId)
                        + " entities=" + std::to_string(entityCount)
                        + " cameras=" + std::to_string(cameraCount)
                        + " freeMem=" + std::to_string(freeMemoryBytes));
                    LastTracedLoadedSceneCount = loadedSceneCount;
                    LastTracedPrimarySceneId = primarySceneId;
                }

            }

            if (ReturnTransitionTraceFramesRemaining > 0) {
                ReturnTransitionTraceFramesRemaining--;
            }

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
