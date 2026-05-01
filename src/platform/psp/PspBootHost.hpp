#pragma once

namespace helengine::psp {
    /// Owns the first PSP native graphics bootstrap and boot-frame presentation loop.
    class PspBootHost {
    public:
        /// Creates the PSP boot host with no initialized graphics state.
        PspBootHost();

        /// Initializes the PSP graphics path and presents the first boot frame until shutdown.
        int Run();

    private:
        /// Initializes the GU state required for future 3D-capable rendering work.
        bool InitializeGraphics();

        /// Presents one solid orange frame to the active display buffer.
        void PresentFrame();

        /// Stores the aligned GU display-list buffer used for command submission.
        unsigned int* DisplayList;
    };
}
