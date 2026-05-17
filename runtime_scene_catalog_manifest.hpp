#pragma once

#include <cstddef>

struct HERuntimeSceneCatalogEntry {
    const char* SceneId;
    const char* CookedRelativePath;
};

const HERuntimeSceneCatalogEntry* he_runtime_scene_catalog_entries(std::size_t* count);
const char* he_runtime_scene_cooked_relative_path(const char* sceneId);
