#include "platform/psp/rendering/PspRuntimeModel.hpp"

#include <utility>

namespace helengine::psp::rendering {
    /// Creates one PSP runtime-model instance.
    PspRuntimeModel::PspRuntimeModel() {
    }

    /// Replaces the cached untextured fixed-function vertex stream used by PSP draws.
    void PspRuntimeModel::SetFixedFunctionVertices(std::vector<FixedFunctionVertex>&& vertices) {
        FixedFunctionVertices = std::move(vertices);
    }

    /// Replaces the cached textured fixed-function vertex stream used by PSP draws.
    void PspRuntimeModel::SetFixedFunctionTexturedVertices(std::vector<FixedFunctionTexturedVertex>&& vertices) {
        FixedFunctionTexturedVertices = std::move(vertices);
    }

    /// Returns whether the runtime model owns one non-empty untextured fixed-function vertex stream.
    bool PspRuntimeModel::HasFixedFunctionVertices() const {
        return !FixedFunctionVertices.empty();
    }

    /// Returns whether the runtime model owns one non-empty textured fixed-function vertex stream.
    bool PspRuntimeModel::HasFixedFunctionTexturedVertices() const {
        return !FixedFunctionTexturedVertices.empty();
    }

    /// Returns the number of cached untextured fixed-function vertices.
    int32_t PspRuntimeModel::GetFixedFunctionVertexCount() const {
        return static_cast<int32_t>(FixedFunctionVertices.size());
    }

    /// Returns the number of cached textured fixed-function vertices.
    int32_t PspRuntimeModel::GetFixedFunctionTexturedVertexCount() const {
        return static_cast<int32_t>(FixedFunctionTexturedVertices.size());
    }

    /// Returns the cached untextured fixed-function vertex stream.
    const PspRuntimeModel::FixedFunctionVertex* PspRuntimeModel::GetFixedFunctionVertices() const {
        return FixedFunctionVertices.empty() ? nullptr : FixedFunctionVertices.data();
    }

    /// Returns the cached textured fixed-function vertex stream.
    const PspRuntimeModel::FixedFunctionTexturedVertex* PspRuntimeModel::GetFixedFunctionTexturedVertices() const {
        return FixedFunctionTexturedVertices.empty() ? nullptr : FixedFunctionTexturedVertices.data();
    }
}
