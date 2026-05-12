#pragma once

class RuntimeSceneCatalog;

namespace helengine::psp {
    /// Builds a generated-core runtime scene catalog from the generated native scene manifest.
    class PspRuntimeSceneCatalogFactory {
    public:
        /// Builds the runtime scene catalog instance consumed by generated core.
        RuntimeSceneCatalog* Build() const;
    };
}
