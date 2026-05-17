#include "runtime/runtime_scene_catalog_manifest.hpp"

#include <cstring>
#include <stdexcept>

static const HERuntimeSceneCatalogEntry kRuntimeSceneCatalogEntries[] = {
    { "DemoDiscMainMenu", "cooked/scenes/DemoDiscMainMenu.hasset" },
    { "axis_test", "cooked/scenes/rendering/axis_test.hasset" },
    { "axis_test2", "cooked/scenes/rendering/axis_test2.hasset" },
    { "colored_cube_grid", "cooked/scenes/rendering/colored_cube_grid.hasset" },
    { "cube_test", "cooked/scenes/rendering/cube_test.hasset" },
    { "directional_shadow_plaza", "cooked/scenes/rendering/directional_shadow_plaza.hasset" },
    { "textured_cube_grid", "cooked/scenes/rendering/textured_cube_grid.hasset" },
};
static const std::size_t kRuntimeSceneCatalogEntryCount = sizeof(kRuntimeSceneCatalogEntries) / sizeof(kRuntimeSceneCatalogEntries[0]);

const HERuntimeSceneCatalogEntry* he_runtime_scene_catalog_entries(std::size_t* count) {
    if (count != nullptr) {
        *count = kRuntimeSceneCatalogEntryCount;
    }

    return kRuntimeSceneCatalogEntries;
}

const char* he_runtime_scene_cooked_relative_path(const char* sceneId) {
    if (sceneId == nullptr || sceneId[0] == '\0') {
        throw std::invalid_argument("Runtime scene id is required.");
    }

    for (std::size_t index = 0; index < kRuntimeSceneCatalogEntryCount; index++) {
        const HERuntimeSceneCatalogEntry& entry = kRuntimeSceneCatalogEntries[index];
        if (std::strcmp(entry.SceneId, sceneId) == 0) {
            return entry.CookedRelativePath;
        }
    }

    throw std::runtime_error("Runtime scene id was not found in the scene catalog manifest.");
}
