#pragma once

#include <pspgu.h>

namespace helengine::psp::rendering {
    /// Completes submitted GU reads before overwriting texture memory, including within an open direct list.
    class PspTextureUpdateBoundary final {
    public:
        /// Finishes the active list before waiting; an unfinished direct list must never be synchronized alone.
        static void SynchronizeBeforeWrite(void* activeDisplayList) {
            if (activeDisplayList != nullptr) {
                sceGuFinish();
            }
            sceGuSync(0, 0);
        }

        /// Starts another segment of the same frame without clearing, swapping or resetting GU render state.
        static void ResumeAfterWrite(void* activeDisplayList) {
            if (activeDisplayList != nullptr) {
                sceGuStart(GU_DIRECT, activeDisplayList);
            }
        }
    };
}
