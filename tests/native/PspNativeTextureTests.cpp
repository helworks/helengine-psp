#include "platform/psp/rendering/PspNativeTexture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    /// Reads one managed fixture without changing the original native byte storage.
    std::vector<std::uint8_t> Load(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw std::runtime_error("Missing fixture: " + path.string());
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), {});
    }
    /// Reads the authored dimensions from a fixture header before invoking the production parser.
    std::uint32_t Word(const std::vector<std::uint8_t>& bytes, int offset) {
        return bytes[offset] | std::uint32_t(bytes[offset + 1]) << 8 | std::uint32_t(bytes[offset + 2]) << 16 | std::uint32_t(bytes[offset + 3]) << 24;
    }
    /// Asserts that malformed headers are refused rather than submitted to GU.
    void Reject(const std::vector<std::uint8_t>& colors, std::size_t paletteBytes, std::uint32_t width, std::uint32_t height) {
        try { helengine::psp::rendering::PspNativeTexture::Read(colors.data(), colors.size(), paletteBytes, width, height); }
        catch (const std::exception&) { return; }
        throw std::runtime_error("Malformed native fixture was accepted.");
    }
}

/// Runs the actual C++ parser and CPU decoder against every cooked managed fixture.
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(argv[1])) {
            if (entry.path().extension() != ".pgt") continue;
            std::filesystem::path prefix = entry.path(); prefix.replace_extension();
            const auto colors = Load(entry.path());
            const auto palette = Load(prefix.string() + ".pal");
            const auto expected = Load(prefix.string() + ".rgba");
            const std::uint32_t width = Word(colors, 16), height = Word(colors, 20);
            const auto layout = helengine::psp::rendering::PspNativeTexture::Read(colors.data(), colors.size(), palette.size(), width, height);
            const auto actual = helengine::psp::rendering::PspNativeTexture::Decode(colors.data(), palette.data(), layout);
            if (expected.size() != width * height * 4) throw std::runtime_error("Wrong managed preview length.");
            for (std::uint32_t y = 0; y < height; y++) {
                for (std::uint32_t x = 0; x < width; x++) {
                    const auto value = actual[y * layout.StorageWidth + x];
                    for (int channel = 0; channel < 4; channel++) {
                        if (((value >> (channel * 8)) & 255) != expected[(y * width + x) * 4 + channel]) throw std::runtime_error("CPU decoder mismatch: " + entry.path().filename().string());
                    }
                }
            }
            for (int offset : { 0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44 }) {
                auto malformed = colors; malformed[offset] = 255; Reject(malformed, palette.size(), width, height);
            }
            auto truncated = colors; truncated.pop_back(); Reject(truncated, palette.size(), width, height);
            count++;
        }
        if (count != 43) throw std::runtime_error("Expected all 43 native combinations.");
        std::cout << "PSP_NATIVE_TEXTURE_TESTS_PASSED fixtures=" << count << " malformed=13-per-fixture\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n'; return 1;
    }
}
