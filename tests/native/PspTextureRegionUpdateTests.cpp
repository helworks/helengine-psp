#include "platform/psp/rendering/PspTextureRegionUpdate.hpp"
#include "platform/psp/rendering/PspTextureUpdateBoundary.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    /// Records the actual production boundary helper's SDK calls without executing GPU commands on the host.
    std::vector<std::string> Events;
    /// Stores the host-owned active list identity expected by the resume callback.
    void* ExpectedList = nullptr;
    /// Rejects a failed semantic assertion with a readable test error.
    void Require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

/// Records completion of the active GU list before waiting for submitted texture reads.
int sceGuFinish() { Events.push_back("finish"); return 0; }
/// Records a fence only after the submitted direct list has been terminated.
int sceGuSync(int, int) { Events.push_back("sync"); return 0; }
/// Verifies the resumed frame uses the same direct-list storage without a clear or swap.
void sceGuStart(int context, void* list) {
    Require(context == GU_DIRECT && list == ExpectedList, "Resume changed the active GU list.");
    Events.push_back("start");
}

/// Checks stride handling, non-mutating validation, retained addresses, and in-frame/out-of-frame ordering.
int main() {
    using helengine::psp::rendering::PspTextureRegionUpdate;
    using helengine::psp::rendering::PspTextureUpdateBoundary;
    std::vector<std::uint32_t> destination(8u * 3u, 0xDEADBEEFu);
    const std::array<std::uint8_t, 20> source = {1,2,3,73, 5,6,7,8, 99,99,99,99, 9,10,11,12, 13,14,15,16};
    const auto originalSource = source;
    const auto* originalAddress = destination.data();
    PspTextureRegionUpdate::Validate(5,3,8,destination.size(),2,1,2,2,source.data(),source.size(),12);
    std::array<int, 4> list = {};
    ExpectedList = list.data();
    PspTextureUpdateBoundary::SynchronizeBeforeWrite(ExpectedList);
    Require(destination[10] == 0xDEADBEEFu, "Destination changed before the GPU fence.");
    PspTextureRegionUpdate::Copy(destination.data(),8,2,1,2,2,source.data(),12);
    Events.push_back("patch");
    PspTextureUpdateBoundary::ResumeAfterWrite(ExpectedList);
    Require(Events == std::vector<std::string>({"finish","sync","patch","start"}), "Open list fence sequence is incorrect.");
    Require(destination.data() == originalAddress, "Region update moved GPU storage.");
    Require(destination[10] == 0x49030201u && destination[11] == 0x08070605u
        && destination[18] == 0x0C0B0A09u && destination[19] == 0x100F0E0Du, "RGBA subrows or alpha bytes were changed.");
    Require(destination[9] == 0xDEADBEEFu && destination[12] == 0xDEADBEEFu
        && destination[15] == 0xDEADBEEFu && destination[23] == 0xDEADBEEFu, "Update changed unrelated texels or padding.");
    Require(source == originalSource, "Source payload was mutated.");
    const auto beforeRejected = destination;
    Events.clear();
    int rejected = 0;
    for (int test = 0; test < 4; ++test) {
        try {
            PspTextureRegionUpdate::Validate(5,3,8,destination.size(),test == 0 ? 4 : 2,1,2,2,
                test == 3 ? nullptr : source.data(),test == 1 ? 19 : source.size(),test == 2 ? 7 : 12);
        } catch (const std::invalid_argument&) { ++rejected; }
    }
    Require(rejected == 4 && destination == beforeRejected && Events.empty(), "Invalid update mutated storage or submitted GPU work.");
    PspTextureUpdateBoundary::SynchronizeBeforeWrite(nullptr);
    Events.push_back("patch");
    PspTextureUpdateBoundary::ResumeAfterWrite(nullptr);
    Require(Events == std::vector<std::string>({"sync","patch"}), "Closed frame unexpectedly started a GU list.");
    std::cout << "PSP region update: 6 semantic checks passed.\n";
}
