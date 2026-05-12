#pragma once

#include <string>

class Asset;
class SceneAsset;

namespace helengine::psp {
    /// Loads cooked packaged assets from the staged PSP homebrew app directory.
    class PspPackagedAssetLoader {
    public:
        /// Creates the packaged asset loader for the supplied app root.
        explicit PspPackagedAssetLoader(const std::string& appRootPath);

        /// Loads one serialized packaged asset by cooked-relative path.
        Asset* LoadAsset(const std::string& cookedRelativePath) const;

        /// Loads the configured startup scene using the generated startup manifest.
        SceneAsset* LoadStartupScene() const;

    private:
        /// Absolute PSP app root that contains the cooked asset tree.
        std::string AppRootPath;
    };
}
