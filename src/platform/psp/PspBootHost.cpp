#include "platform/psp/PspBootHost.hpp"

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <new>
#include <string>

#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspfpu.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspuser.h>

#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "Entity.hpp"
#include "IAudioBackend.hpp"
#include "IRuntimeDiagnosticsProvider.hpp"
#include "IRuntimeUpdateStageDiagnosticsProvider.hpp"
#include "BepuPhysicsWorld3D.hpp"
#include "BepuRuntimeComponentRegistration.hpp"
#include "InputControlId.hpp"
#include "InputControlKind.hpp"
#include "InputDeviceKind.hpp"
#include "LoadedSceneRecord.hpp"
#include "ObjectManager.hpp"
#include "PlatformInfo.hpp"
#include "RenderManager2D.hpp"
#include "RenderManager3D.hpp"
#include "RuntimeSceneLoadService.hpp"
#include "RuntimeMemoryDiagnosticsSnapshot.hpp"
#include "SceneEntityRuntimeIdComponent.hpp"
#include "SceneManager.hpp"
#include "SceneAsset.hpp"
#include "StandardPlatformAction.hpp"
#include "StandardPlatformActionBinding.hpp"
#include "StandardPlatformInputConfiguration.hpp"
#include "platform/psp/PspAppRootPathResolver.hpp"
#include "platform/psp/PspBootTrace.hpp"
#include "platform/psp/PspInputBackend.hpp"
#include "platform/psp/PspMemoryCardContentStreamSource.hpp"
#include "platform/psp/PspPackagedAssetLoader.hpp"
#include "platform/psp/PspRuntimeSceneCatalogFactory.hpp"
#include "platform/psp/audio/PspAudioBackend.hpp"
#include "platform/psp/rendering/PspRenderManager2D.hpp"
#include "platform/psp/rendering/PspRenderManager3D.hpp"
#include "runtime/native_exceptions.hpp"
#include "runtime/native_list.hpp"

#if defined(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP) && HELENGINE_PSP_ENABLE_RUNTIME_STARTUP
#include "runtime/runtime_scene_catalog_manifest.hpp"
#include "runtime/runtime_startup_manifest.hpp"
#include "runtime/runtime_standard_platform_input_manifest.hpp"
#endif

PSP_MODULE_INFO("helengine_psp", 0, 1, 0);
// Scene transitions can briefly overlap two large cooked PCM music assets, so the PSP app heap
// needs headroom beyond the default 16 MB budget.
PSP_HEAP_SIZE_KB(24 * 1024);
PSP_MAIN_THREAD_STACK_SIZE_KB(512);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU | THREAD_ATTR_USER);

namespace helengine::psp {
    namespace {
        constexpr int BufferWidth = 512;
        constexpr int BufferHeight = 272;
        constexpr int ScreenWidth = 480;
        constexpr int ScreenHeight = BufferHeight;
        constexpr std::size_t DisplayListByteCount = 0x80000;
        constexpr unsigned int DefaultClearColor = 0xFF101010;
        constexpr const char* GraphicsInitializationStageName = "GraphicsInitialization";
        constexpr const char* IsolatedFrameLoopStageName = "IsolatedFrameLoop";
        constexpr const char* RuntimeAppRootStageName = "RuntimeAppRootResolution";
        constexpr const char* RuntimeCoreInitializationStageName = "RuntimeCoreInitialization";
        constexpr const char* RuntimeStartupSceneAssetLoadStageName = "RuntimeStartupSceneAssetLoad";
        constexpr const char* RuntimeStartupSceneMaterializationStageName = "RuntimeStartupSceneMaterialization";
        constexpr const char* RuntimeMainLoopStageName = "RuntimeMainLoop";

        alignas(64) unsigned int DisplayListStorage[DisplayListByteCount / sizeof(unsigned int)];

        /// Configures the PSP main-thread FPU to retain IEEE exceptional results as flags instead of taking hardware exceptions.
        void ConfigureFloatingPointEnvironment() {
            const uint32_t fcr31Before = pspFpuGetFCR31();
            pspFpuSetEnable(0);
            pspFpuClearFlags(PSP_FPU_EXCEPTION_ALL);
            pspFpuClearCause(PSP_FPU_EXCEPTION_ALL);
            const uint32_t fcr31After = pspFpuGetFCR31();
            PspBootTrace::WriteLine(
                std::string("PspFloatingPoint FCR31Before=") + std::to_string(fcr31Before)
                + " FCR31After=" + std::to_string(fcr31After)
                + " ExceptionEnables=" + std::to_string(pspFpuGetEnable()));
        }

        /// Writes a bounded sequence of core update stages after scene physics binding so a hard crash can be localized without continuous memory-card logging.
        class PspPhysicsUpdateStageDiagnosticsProvider final
            : public ::IRuntimeDiagnosticsProvider
            , public ::IRuntimeUpdateStageDiagnosticsProvider {
        public:
            /// Creates an empty platform memory snapshot because the diagnostic provider is only used to trace core update stages.
            ::RuntimeMemoryDiagnosticsSnapshot* CaptureSnapshot() override {
                return new ::RuntimeMemoryDiagnosticsSnapshot();
            }

            /// Begins recording the first core update transitions that follow completed scene physics binding.
            void BeginPostSceneBindingTrace() {
                if (IsPostSceneBindingTraceActive) {
                    return;
                }

                IsPostSceneBindingTraceActive = true;
                RecordedPhysicsStageCount = 0;
                ObjectUpdateMicroseconds = 0;
                PhysicsMicroseconds = 0;
                BepuTimestepMicroseconds = 0;
                BepuSynchronizationMicroseconds = 0;
                AudioMicroseconds = 0;
                FrameCount = 0;
                PspBootTrace::WriteLine("PspPhysicsProfiler post-scene-binding capture enabled.");
            }

            /// Records one core update-stage transition after physics binding without adding per-frame memory-card writes after the diagnostic limit.
            void ReportUpdateStage(std::string stage) override {
                if (!IsPostSceneBindingTraceActive) {
                    return;
                }

                const std::uint64_t nowMicroseconds = static_cast<std::uint64_t>(sceKernelGetSystemTimeWide());
                if (stage == "BeforeObjectManagerUpdate") {
                    ObjectUpdateStartMicroseconds = nowMicroseconds;
                } else if (stage == "AfterObjectManagerUpdate") {
                    ObjectUpdateMicroseconds += nowMicroseconds - ObjectUpdateStartMicroseconds;
                } else if (stage == "BeforeUpdatePhysics") {
                    PhysicsStartMicroseconds = nowMicroseconds;
                } else if (stage == "AfterUpdatePhysics") {
                    PhysicsMicroseconds += nowMicroseconds - PhysicsStartMicroseconds;
                } else if (stage == "BeforeBepuTimestep") {
                    BepuTimestepStartMicroseconds = nowMicroseconds;
                } else if (stage == "AfterBepuTimestepBeforeSync") {
                    BepuTimestepMicroseconds += nowMicroseconds - BepuTimestepStartMicroseconds;
                } else if (stage == "BeforeBepuSynchronizeBodies") {
                    BepuSynchronizationStartMicroseconds = nowMicroseconds;
                } else if (stage == "AfterBepuSync") {
                    BepuSynchronizationMicroseconds += nowMicroseconds - BepuSynchronizationStartMicroseconds;
                } else if (stage == "BeforeAudioManagerUpdate") {
                    AudioStartMicroseconds = nowMicroseconds;
                } else if (stage == "AfterAudioManagerUpdate") {
                    AudioMicroseconds += nowMicroseconds - AudioStartMicroseconds;
                } else if (stage == "BeforeInputEarlyUpdate") {
                    FrameCount++;
                    if ((FrameCount % 30) == 0) {
                        PspBootTrace::WriteLine(
                            std::string("PspPhysicsProfiler frames=") + std::to_string(FrameCount)
                            + " objectUpdateUs=" + std::to_string(ObjectUpdateMicroseconds / 30)
                            + " physicsUs=" + std::to_string(PhysicsMicroseconds / 30)
                            + " bepuTimestepUs=" + std::to_string(BepuTimestepMicroseconds / 30)
                            + " bepuSyncUs=" + std::to_string(BepuSynchronizationMicroseconds / 30)
                            + " audioUs=" + std::to_string(AudioMicroseconds / 30));
                        ObjectUpdateMicroseconds = 0;
                        PhysicsMicroseconds = 0;
                        BepuTimestepMicroseconds = 0;
                        BepuSynchronizationMicroseconds = 0;
                        AudioMicroseconds = 0;
                    }
                }
            }

        private:
            /// Stores whether the trace has been armed by the synchronous scene physics-binding completion event.
            bool IsPostSceneBindingTraceActive = false;
            /// Stores the number of physics update-stage records already persisted for this runtime session.
            int32_t RecordedPhysicsStageCount = 0;
            std::uint64_t ObjectUpdateStartMicroseconds = 0;
            std::uint64_t PhysicsStartMicroseconds = 0;
            std::uint64_t BepuTimestepStartMicroseconds = 0;
            std::uint64_t BepuSynchronizationStartMicroseconds = 0;
            std::uint64_t AudioStartMicroseconds = 0;
            std::uint64_t ObjectUpdateMicroseconds = 0;
            std::uint64_t PhysicsMicroseconds = 0;
            std::uint64_t BepuTimestepMicroseconds = 0;
            std::uint64_t BepuSynchronizationMicroseconds = 0;
            std::uint64_t AudioMicroseconds = 0;
            int32_t FrameCount = 0;
        };

        /// Stores the one diagnostics provider owned by the active PSP core so the non-capturing scene-binding callback can arm its post-binding trace.
        PspPhysicsUpdateStageDiagnosticsProvider* PostSceneBindingTraceDiagnosticsProvider = nullptr;

        /// Writes a bounded snapshot of runtime scene entity identifiers after a fatal error so unresolved scene references can be traced without per-frame logging.
        void WriteSceneReferenceDiagnostics(::Core* engineCore) {
            constexpr int32_t MaximumSceneReferenceRecordCount = 512;
            if (engineCore == nullptr || engineCore->get_ObjectManager() == nullptr) {
                PspBootTrace::WriteLine("SceneReferenceDump unavailable: object manager is not initialized.");
                return;
            }

            ::List<::Entity*>* entities = engineCore->get_ObjectManager()->get_Entities();
            if (entities == nullptr) {
                PspBootTrace::WriteLine("SceneReferenceDump unavailable: entity collection is not initialized.");
                return;
            }

            int32_t runtimeIdComponentCount = 0;
            int32_t targetEntityRecordCount = 0;
            PspBootTrace::WriteLine(
                std::string("SceneReferenceDump begin entityCount=")
                + std::to_string(entities->get_Count()));

            for (int32_t entityIndex = 0; entityIndex < entities->get_Count(); entityIndex++) {
                ::Entity* entity = entities->get_Item(entityIndex);
                if (entity == nullptr || entity->get_Components() == nullptr) {
                    continue;
                }

                ::List<::Component*>* components = entity->get_Components();
                for (int32_t componentIndex = 0; componentIndex < components->get_Count(); componentIndex++) {
                    ::SceneEntityRuntimeIdComponent* runtimeIdComponent = dynamic_cast<::SceneEntityRuntimeIdComponent*>(components->get_Item(componentIndex));
                    if (runtimeIdComponent == nullptr) {
                        continue;
                    }

                    runtimeIdComponentCount++;
                    if (runtimeIdComponent->get_SceneEntityId() == 39u) {
                        targetEntityRecordCount++;
                    }

                    if (runtimeIdComponentCount > MaximumSceneReferenceRecordCount) {
                        continue;
                    }

                    PspBootTrace::WriteLine(
                        std::string("SceneReferenceId entityIndex=")
                        + std::to_string(entityIndex)
                        + " componentIndex=" + std::to_string(componentIndex)
                        + " id=" + std::to_string(runtimeIdComponent->get_SceneEntityId())
                        + " componentCount=" + std::to_string(components->get_Count()));
                }
            }

            PspBootTrace::WriteLine(
                std::string("SceneReferenceDump end runtimeIdCount=")
                + std::to_string(runtimeIdComponentCount)
                + " targetId39Count=" + std::to_string(targetEntityRecordCount)
                + " recordLimit=" + std::to_string(MaximumSceneReferenceRecordCount));
        }

        /// Builds one runtime standard-platform input configuration from the generated manifest entries.
        ::StandardPlatformInputConfiguration* BuildStandardPlatformInputConfiguration() {
#if defined(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP) && HELENGINE_PSP_ENABLE_RUNTIME_STARTUP
            std::size_t count = 0;
            const HERuntimeStandardPlatformActionEntry* manifestEntries = he_runtime_standard_platform_action_entries(&count);
            if (manifestEntries == nullptr || count == 0) {
                return ::StandardPlatformInputConfiguration::get_Empty();
            }

            List<::StandardPlatformActionBinding*>* bindings = new List<::StandardPlatformActionBinding*>(static_cast<int32_t>(count));
            for (int32_t index = 0; index < static_cast<int32_t>(count); index++) {
                const HERuntimeStandardPlatformActionEntry& manifestEntry = manifestEntries[index];
                bindings->Add(new ::StandardPlatformActionBinding(
                    static_cast<::StandardPlatformAction>(manifestEntry.ActionId),
                    ::InputControlId(
                        static_cast<::InputDeviceKind>(manifestEntry.DeviceKind),
                        static_cast<::InputControlKind>(manifestEntry.ControlKind),
                        manifestEntry.DeviceIndex,
                        manifestEntry.ControlIndex)));
            }

            return new ::StandardPlatformInputConfiguration(bindings);
#else
            return ::StandardPlatformInputConfiguration::get_Empty();
#endif
        }
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
          EngineAudioBackend(nullptr),
          LastTracedLoadedSceneCount(-1),
          LastTracedPrimarySceneId(),
          ExitRequested(false),
          ExitCallbackId(-1),
          ExitCallbackThreadId(-1) {
    }

    /// Initializes the PSP runtime and presents frames until shutdown.
    int PspBootHost::Run() {
        try {
            PspBootTrace::WriteLine("Run begin");
            ConfigureFloatingPointEnvironment();
            RegisterExitCallback();
            EnterBootStage(GraphicsInitializationStageName);
            if (!InitializeGraphics()) {
                PspBootTrace::WriteLine("InitializeGraphics returned false");
                return 1;
            }
            CompleteBootStage();

#if defined(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP) && HELENGINE_PSP_ENABLE_RUNTIME_STARTUP
            PspBootTrace::WriteLine("Checkpointed runtime startup enabled.");
            RunCheckpointedStartup();
            PspBootTrace::WriteLine("Run exit requested. Calling sceKernelExitGame.");
            sceKernelExitGame();
            return 0;
#elif defined(HELENGINE_PSP_ISOLATED_BOOT) && HELENGINE_PSP_ISOLATED_BOOT
            PspBootTrace::WriteLine("Isolated boot enabled. Skipping generated-core runtime startup.");
            RunIsolatedFrameLoop();
            PspBootTrace::WriteLine("Run exit requested. Calling sceKernelExitGame.");
            sceKernelExitGame();
            return 0;
#else
            PspBootTrace::WriteLine("No explicit runtime-startup mode was selected. Falling back to isolated boot.");
            RunIsolatedFrameLoop();
            PspBootTrace::WriteLine("Run exit requested. Calling sceKernelExitGame.");
            sceKernelExitGame();
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
        sceGuEnable(GU_CLIP_PLANES);
        sceGuEnable(GU_SCISSOR_TEST);
        sceGuScissor(0, 0, ScreenWidth, ScreenHeight);
        sceGuDisable(GU_CULL_FACE);
        sceGuDisable(GU_TEXTURE_2D);
        sceGuShadeModel(GU_SMOOTH);
        sceGuFinish();
        sceGuSync(0, 0);
        sceGuDisplay(GU_TRUE);
        PspBootTrace::WriteLine("PSP_DIRECTIONAL_DIAG build=2026-05-21T1624 trace=clip-planes-full-scissor");
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
        EngineOptions->set_ContentStreamSource(new PspMemoryCardContentStreamSource(appRootPath));
        EngineOptions->set_UpdateOrderLayers(4);
        EngineOptions->set_RenderOrderLayers3D(4);
        EngineOptions->set_UpdateListInitialCapacity(64);
        EngineOptions->set_RenderList2DInitialCapacity(8);
        EngineOptions->set_RenderList3DInitialCapacity(64);
        EngineOptions->set_PhysicsFixedStepSeconds(1.0 / 12.0);
        EngineOptions->set_PhysicsMaxStepsPerUpdate(1);
#if defined(HELENGINE_PSP_ENABLE_BOOT_TRACE) && HELENGINE_PSP_ENABLE_BOOT_TRACE
        PostSceneBindingTraceDiagnosticsProvider = new PspPhysicsUpdateStageDiagnosticsProvider();
        EngineOptions->set_RuntimeDiagnosticsProvider(PostSceneBindingTraceDiagnosticsProvider);
#endif
        EngineOptions->set_StandardPlatformInputConfiguration(BuildStandardPlatformInputConfiguration());
        PspRuntimeSceneCatalogFactory runtimeSceneCatalogFactory;
        EngineOptions->set_SceneCatalog(runtimeSceneCatalogFactory.Build());

        rendering::PspRenderManager3D* pspRenderManager3D = new rendering::PspRenderManager3D();
        rendering::PspRenderManager2D* pspRenderManager2D = new rendering::PspRenderManager2D();
        pspRenderManager3D->SetRenderManager2D(pspRenderManager2D);
        EngineRenderManager3D = pspRenderManager3D;
        EngineRenderManager2D = pspRenderManager2D;
        EngineInputBackend = new PspInputBackend();
        EngineAudioBackend = new PspAudioBackend();

        EngineRenderManager3D->AddWindow(0, ScreenWidth, ScreenHeight);
        PlatformInfo* platformInfo = BuildRuntimePlatformInfo();
        EngineCore->Initialize(
            EngineRenderManager3D,
            EngineRenderManager2D,
            EngineInputBackend,
            platformInfo,
            EngineOptions);
        EngineCore->SetAudioBackend(EngineAudioBackend);
        BepuPhysicsWorld3D* physicsWorld = BepuPhysicsWorld3D::CreateWithSolveSchedule(1, 1);
        physicsWorld->set_SceneBindingDiagnosticSink(
            new Action<std::string>([](std::string message) {
                PspBootTrace::WriteLine(std::string("PhysicsBinding ") + message);
                if (message.find("PhysicsBind end bodies=") == 0 && PostSceneBindingTraceDiagnosticsProvider != nullptr) {
                    PostSceneBindingTraceDiagnosticsProvider->BeginPostSceneBindingTrace();
                    rendering::PspRenderManager3D::BeginPostPhysicsBindingDrawTrace();
                }
            }));
        BepuRuntimeComponentRegistration::AttachRuntimeWorld(EngineCore, physicsWorld);
        BepuRuntimeComponentRegistration::RegisterSceneBinding(EngineCore);

        CompleteBootStage();
    }

    /// Loads the configured startup scene through the runtime scene manager so scene lifetime stays tracked from frame one.
    void PspBootHost::LoadStartupScene() {
#if defined(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP) && HELENGINE_PSP_ENABLE_RUNTIME_STARTUP
        EnterBootStage(RuntimeStartupSceneAssetLoadStageName);
        const char* configuredStartupSceneRelativePath = he_get_runtime_startup_scene_relative_path();
        if (configuredStartupSceneRelativePath == nullptr || configuredStartupSceneRelativePath[0] == '\0') {
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
            if (runtimeSceneEntry.CookedRelativePath != nullptr && std::string(runtimeSceneEntry.CookedRelativePath) == configuredStartupSceneRelativePath) {
                startupSceneId = runtimeSceneEntry.SceneId;
                break;
            }
        }

        if (startupSceneId.empty()) {
            throw std::runtime_error("PSP runtime startup scene path was not found in the runtime scene catalog manifest.");
        }

        std::string overrideStartupSceneId = TryReadStartupSceneOverrideSceneId();
        if (!overrideStartupSceneId.empty()) {
            startupSceneId = overrideStartupSceneId;
        }

        std::string startupSceneRelativePath = configuredStartupSceneRelativePath;
        for (std::size_t index = 0; index < runtimeSceneCount; index++) {
            const HERuntimeSceneCatalogEntry& runtimeSceneEntry = runtimeSceneEntries[index];
            if (runtimeSceneEntry.SceneId != nullptr && startupSceneId == runtimeSceneEntry.SceneId) {
                startupSceneRelativePath = runtimeSceneEntry.CookedRelativePath != nullptr
                    ? std::string(runtimeSceneEntry.CookedRelativePath)
                    : std::string();
                break;
            }
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

    /// Reads one optional startup-scene override id from the packaged app root for runtime diagnostics.
    std::string PspBootHost::TryReadStartupSceneOverrideSceneId() const {
        if (AppRootPath.empty()) {
            return std::string();
        }

        std::string overrideFilePath = AppRootPath + "/startup_scene_override.txt";
        std::FILE* file = std::fopen(overrideFilePath.c_str(), "rb");
        if (file == nullptr) {
            return std::string();
        }

        char buffer[256] = {};
        std::size_t byteCount = std::fread(buffer, 1, sizeof(buffer) - 1, file);
        std::fclose(file);
        if (byteCount == 0) {
            return std::string();
        }

        std::string sceneId(buffer, byteCount);
        std::size_t startIndex = 0;
        while (startIndex < sceneId.size() && std::isspace(static_cast<unsigned char>(sceneId[startIndex])) != 0) {
            startIndex++;
        }

        std::size_t endIndex = sceneId.size();
        while (endIndex > startIndex && std::isspace(static_cast<unsigned char>(sceneId[endIndex - 1])) != 0) {
            endIndex--;
        }

        std::string trimmedSceneId = sceneId.substr(startIndex, endIndex - startIndex);
        if (!trimmedSceneId.empty()) {
            PspBootTrace::WriteLine(std::string("StartupSceneOverride sceneId=") + trimmedSceneId);
        }

        return trimmedSceneId;
    }

    /// Returns the current primary loaded-scene id, or an empty string when no scene is active.
    std::string PspBootHost::GetPrimarySceneId() const {
        if (EngineCore == nullptr || EngineCore->get_SceneManager() == nullptr) {
            return std::string();
        }

        IReadOnlyList<LoadedSceneRecord*>* loadedScenes = EngineCore->get_SceneManager()->get_LoadedScenes();
        if (loadedScenes == nullptr || loadedScenes->get_Count() <= 0 || loadedScenes->get_Item(0) == nullptr) {
            return std::string();
        }

        return loadedScenes->get_Item(0)->get_SceneId();
    }

    /// Returns the current runtime loaded-scene count.
    int32_t PspBootHost::GetLoadedSceneCount() const {
        if (EngineCore == nullptr || EngineCore->get_SceneManager() == nullptr) {
            return 0;
        }

        IReadOnlyList<LoadedSceneRecord*>* loadedScenes = EngineCore->get_SceneManager()->get_LoadedScenes();
        if (loadedScenes == nullptr) {
            return 0;
        }

        return loadedScenes->get_Count();
    }

    /// Registers the PSP kernel exit callback so the Home button can request a clean shutdown.
    void PspBootHost::RegisterExitCallback() {
        if (ExitCallbackThreadId >= 0) {
            return;
        }

        ExitCallbackThreadId = sceKernelCreateThread(
            "helengine_exit_callback_thread",
            &PspBootHost::ExitCallbackThreadEntry,
            0x11,
            0x1000,
            PSP_THREAD_ATTR_USER,
            nullptr);
        if (ExitCallbackThreadId < 0) {
            throw std::runtime_error("Failed to create the PSP exit callback thread.");
        }

        PspBootHost* host = this;
        int startResult = sceKernelStartThread(ExitCallbackThreadId, sizeof(PspBootHost*), &host);
        if (startResult < 0) {
            sceKernelDeleteThread(ExitCallbackThreadId);
            ExitCallbackThreadId = -1;
            throw std::runtime_error("Failed to start the PSP exit callback thread.");
        }
    }

    /// Returns whether the PSP Home button requested application shutdown.
    bool PspBootHost::IsExitRequested() const {
        return ExitRequested.load();
    }

    /// Handles the PSP kernel exit callback and records one shutdown request on the owning host.
    int PspBootHost::HandleExitCallback(int argument1, int argument2, void* common) {
        (void)argument1;
        (void)argument2;

        PspBootHost* host = static_cast<PspBootHost*>(common);
        if (host != nullptr) {
            host->ExitRequested.store(true);
        }

        PspBootTrace::WriteLine("Home button exit requested.");
        return 0;
    }

    /// Owns the dedicated PSP callback thread that registers the Home-button exit callback.
    int PspBootHost::ExitCallbackThreadEntry(SceSize argumentsSize, void* arguments) {
        if (arguments == nullptr || argumentsSize != sizeof(PspBootHost*)) {
            return 0;
        }

        PspBootHost* host = *reinterpret_cast<PspBootHost**>(arguments);
        if (host == nullptr) {
            return 0;
        }

        host->ExitCallbackId = sceKernelCreateCallback(
            "helengine_exit_callback",
            &PspBootHost::HandleExitCallback,
            host);
        if (host->ExitCallbackId < 0) {
            return 0;
        }

        sceKernelRegisterExitCallback(host->ExitCallbackId);
        sceKernelSleepThreadCB();
        return 0;
    }

    /// Runs the normal generated-core update and draw loop after startup succeeds.
    void PspBootHost::RunMainLoop() {
        EnterBootStage(RuntimeMainLoopStageName);
        CompleteBootStage();

        while (!IsExitRequested()) {
            try {
                EngineCore->Update();
            } catch (const std::bad_alloc&) {
                throw;
            }

            SceneManager* sceneManager = EngineCore->get_SceneManager();
            if (sceneManager != nullptr) {
                IReadOnlyList<LoadedSceneRecord*>* loadedScenes = sceneManager->get_LoadedScenes();
                const int32_t loadedSceneCount = loadedScenes != nullptr ? loadedScenes->get_Count() : 0;
                std::string primarySceneId;
                if (loadedScenes != nullptr && loadedSceneCount > 0 && loadedScenes->get_Item(0) != nullptr) {
                    primarySceneId = loadedScenes->get_Item(0)->get_SceneId();
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

            BeginFrame();
            EngineCore->Draw();
            PresentFrame();
        }

        PspBootTrace::WriteLine("RunMainLoop observed exit request.");
    }

    /// Presents a stable blank frame continuously while generated-core runtime startup is isolated.
    void PspBootHost::RunIsolatedFrameLoop() {
        EnterBootStage(IsolatedFrameLoopStageName);
        CompleteBootStage();

        while (!IsExitRequested()) {
            BeginFrame();
            PresentFrame();
        }

        PspBootTrace::WriteLine("RunIsolatedFrameLoop observed exit request.");
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
        WriteSceneReferenceDiagnostics(EngineCore);
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
