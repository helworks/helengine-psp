#include "platform/psp/PspPackagedAssetLoader.hpp"

#include <stdexcept>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "SceneAsset.hpp"
#include "platform/psp/PspBootTrace.hpp"
#include "runtime/runtime_startup_manifest.hpp"
#include "system/io/file.hpp"

namespace helengine::psp {
    namespace {
        /// Normalizes one app-root path to forward slashes without a trailing separator.
        std::string NormalizeAppRootPath(const std::string& value) {
            if (value.empty()) {
                throw std::invalid_argument("App root path is required.");
            }

            std::string normalized = value;
            for (std::size_t index = 0; index < normalized.size(); index++) {
                if (normalized[index] == '\\') {
                    normalized[index] = '/';
                }
            }

            while (normalized.size() > 1 && normalized.back() == '/') {
                normalized.pop_back();
            }

            return normalized;
        }

        /// Normalizes one cooked-relative path to forward slashes.
        std::string NormalizeRelativePath(const std::string& value) {
            if (value.empty()) {
                throw std::invalid_argument("Cooked-relative path is required.");
            }

            std::string normalized = value;
            for (std::size_t index = 0; index < normalized.size(); index++) {
                if (normalized[index] == '\\') {
                    normalized[index] = '/';
                }
            }

            if (normalized[0] == '/') {
                throw std::invalid_argument("Cooked-relative path must not be rooted.");
            }
            if (normalized.find("..") != std::string::npos) {
                throw std::invalid_argument("Cooked-relative path must stay inside the PSP app root.");
            }

            return normalized;
        }
    }

    /// Creates the packaged asset loader for the supplied app root.
    PspPackagedAssetLoader::PspPackagedAssetLoader(const std::string& appRootPath)
        : AppRootPath(NormalizeAppRootPath(appRootPath)) {
    }

    /// Loads one serialized packaged asset by cooked-relative path.
    Asset* PspPackagedAssetLoader::LoadAsset(const std::string& cookedRelativePath) const {
        std::string normalizedRelativePath = NormalizeRelativePath(cookedRelativePath);
        std::string fullPath = AppRootPath + "/" + normalizedRelativePath;
        PspBootTrace::WriteLine("LoadAsset path=" + fullPath);
        if (!File::Exists(fullPath)) {
            PspBootTrace::WriteLine("LoadAsset missing=" + fullPath);
            throw std::runtime_error("Packaged PSP asset was not found: " + fullPath);
        }

        FileStream* stream = File::OpenRead(fullPath);
        if (stream == nullptr) {
            PspBootTrace::WriteLine("LoadAsset open failed=" + fullPath);
            throw std::runtime_error("Packaged PSP asset could not be opened: " + fullPath);
        }

        Asset* asset = AssetSerializer::Deserialize(stream);
        delete stream;
        PspBootTrace::WriteLine("LoadAsset success=" + fullPath);
        return asset;
    }

    /// Loads the configured startup scene using the generated startup manifest.
    SceneAsset* PspPackagedAssetLoader::LoadStartupScene() const {
        const char* startupSceneRelativePath = he_get_runtime_startup_scene_relative_path();
        if (startupSceneRelativePath == nullptr || startupSceneRelativePath[0] == '\0') {
            throw std::runtime_error("PSP runtime startup manifest did not define a startup scene.");
        }

        return static_cast<SceneAsset*>(LoadAsset(startupSceneRelativePath));
    }
}
