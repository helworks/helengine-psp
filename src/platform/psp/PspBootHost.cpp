#include "platform/psp/PspBootHost.hpp"

#include <pspdisplay.h>
#include <pspgu.h>
#include <pspuser.h>

PSP_MODULE_INFO("helengine_psp", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU | THREAD_ATTR_USER);

namespace helengine::psp {
    namespace {
        constexpr int BufferWidth = 512;
        constexpr int BufferHeight = 272;
        constexpr int ScreenWidth = 480;
        constexpr int ScreenHeight = BufferHeight;
        constexpr unsigned int OrangeClearColor = 0xFF00A5FF;

        alignas(64) unsigned int DisplayListStorage[0x20000 / sizeof(unsigned int)];
    }

    /// Creates the PSP boot host with no initialized graphics state.
    PspBootHost::PspBootHost()
        : DisplayList(DisplayListStorage) {
    }

    /// Initializes the PSP graphics path and presents the first boot frame until shutdown.
    int PspBootHost::Run() {
        if (!InitializeGraphics()) {
            return 1;
        }

        while (true) {
            PresentFrame();
        }

        return 0;
    }

    /// Initializes the GU state required for future 3D-capable rendering work.
    bool PspBootHost::InitializeGraphics() {
        sceGuInit();
        sceGuStart(GU_DIRECT, DisplayList);
        sceGuDrawBuffer(GU_PSM_8888, reinterpret_cast<void*>(0), BufferWidth);
        sceGuDispBuffer(ScreenWidth, ScreenHeight, reinterpret_cast<void*>(0x88000), BufferWidth);
        sceGuDepthBuffer(reinterpret_cast<void*>(0x110000), BufferWidth);
        sceGuOffset(2048 - (ScreenWidth / 2), 2048 - (ScreenHeight / 2));
        sceGuViewport(2048, 2048, ScreenWidth, ScreenHeight);
        sceGuEnable(GU_SCISSOR_TEST);
        sceGuScissor(0, 0, ScreenWidth, ScreenHeight);
        sceGuDepthRange(65535, 0);
        sceGuDepthFunc(GU_GEQUAL);
        sceGuEnable(GU_DEPTH_TEST);
        sceGuFinish();
        sceGuDisplay(GU_TRUE);

        return true;
    }

    /// Presents one solid orange frame to the active display buffer.
    void PspBootHost::PresentFrame() {
        sceGuStart(GU_DIRECT, DisplayList);
        sceGuClearColor(OrangeClearColor);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }
}
