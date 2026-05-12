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
