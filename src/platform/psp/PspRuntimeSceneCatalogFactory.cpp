#include "platform/psp/PspRuntimeSceneCatalogFactory.hpp"

#include <stdexcept>

#if __has_include("RuntimeSceneCatalog.hpp")
#include "RuntimeSceneCatalog.hpp"
#include "RuntimeSceneCatalogEntry.hpp"
#include "runtime/array.hpp"
#if defined(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP) && HELENGINE_PSP_ENABLE_RUNTIME_STARTUP
#include "runtime/runtime_scene_catalog_manifest.hpp"
#endif
#endif

namespace helengine::psp {
    /// Builds the runtime scene catalog instance consumed by generated core.
    RuntimeSceneCatalog* PspRuntimeSceneCatalogFactory::Build() const {
#if __has_include("RuntimeSceneCatalog.hpp")
#if defined(HELENGINE_PSP_ENABLE_RUNTIME_STARTUP) && HELENGINE_PSP_ENABLE_RUNTIME_STARTUP
        std::size_t nativeEntryCount = 0;
        const HERuntimeSceneCatalogEntry* nativeEntries = he_runtime_scene_catalog_entries(&nativeEntryCount);
        if (nativeEntries == nullptr || nativeEntryCount == 0) {
            throw std::runtime_error("PSP runtime scene catalog manifest did not contain any entries.");
        }

        Array<::RuntimeSceneCatalogEntry*>* sceneEntries = new Array<::RuntimeSceneCatalogEntry*>(static_cast<int32_t>(nativeEntryCount));
        for (std::size_t index = 0; index < nativeEntryCount; index++) {
            const HERuntimeSceneCatalogEntry& nativeEntry = nativeEntries[index];
            (*sceneEntries)[static_cast<int32_t>(index)] = new ::RuntimeSceneCatalogEntry(nativeEntry.SceneId, nativeEntry.CookedRelativePath);
        }

        return new RuntimeSceneCatalog(sceneEntries);
#else
        throw std::runtime_error("Runtime scene catalog generation is only available when PSP runtime startup is enabled.");
#endif
#else
        throw std::runtime_error("Generated core does not expose RuntimeSceneCatalog for PSP scene management.");
#endif
    }
}
