#include "runtime/runtime_startup_manifest.hpp"

static const char kRuntimeStartupSceneRelativePath[] = "cooked/scenes/rendering/colored_cube_grid.hasset";
static const char kRuntimePlatformName[] = "psp";
static const char kRuntimePlatformVersion[] = "local";
static const bool kRuntimeDynamicBatchingEnabled = true;

const char* he_get_runtime_startup_scene_relative_path() {
    return kRuntimeStartupSceneRelativePath;
}

const char* he_get_runtime_platform_name() {
    return kRuntimePlatformName;
}

const char* he_get_runtime_platform_version() {
    return kRuntimePlatformVersion;
}

bool he_get_runtime_dynamic_batching_enabled() {
    return kRuntimeDynamicBatchingEnabled;
}
