#include "platform/psp/PspBootHost.hpp"
#include "platform/psp/PspBootTrace.hpp"

#include <string>

int main(int argc, char** argv) {
    std::string executablePath;
    if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
        executablePath = argv[0];
    }

    helengine::psp::PspBootTrace::WriteLine("main entered executablePath=" + executablePath);
    helengine::psp::PspBootHost host(executablePath);
    return host.Run();
}
