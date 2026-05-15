#pragma once

#include <cstdint>
#include <vector>

#include "RuntimeModel.hpp"

namespace helengine::psp::rendering {
    /// Exposes one concrete PSP runtime-model type so generated core can instantiate authored mesh data for the renderer.
    class PspRuntimeModel final : public RuntimeModel {
    public:
        /// Stores one ready-to-submit untextured fixed-function PSP vertex.
        struct FixedFunctionVertex {
            /// Stores the local-space normal X component submitted to GU.
            float NX;

            /// Stores the local-space normal Y component submitted to GU.
            float NY;

            /// Stores the local-space normal Z component submitted to GU.
            float NZ;

            /// Stores the local-space position X component submitted to GU.
            float X;

            /// Stores the local-space position Y component submitted to GU.
            float Y;

            /// Stores the local-space position Z component submitted to GU.
            float Z;
        };

        /// Stores one ready-to-submit textured fixed-function PSP vertex.
        struct FixedFunctionTexturedVertex {
            /// Stores the U texture coordinate submitted to GU.
            float U;

            /// Stores the V texture coordinate submitted to GU.
            float V;

            /// Stores the local-space normal X component submitted to GU.
            float NX;

            /// Stores the local-space normal Y component submitted to GU.
            float NY;

            /// Stores the local-space normal Z component submitted to GU.
            float NZ;

            /// Stores the local-space position X component submitted to GU.
            float X;

            /// Stores the local-space position Y component submitted to GU.
            float Y;

            /// Stores the local-space position Z component submitted to GU.
            float Z;
        };

        /// Creates one PSP runtime-model instance.
        PspRuntimeModel();

        /// Replaces the cached untextured fixed-function vertex stream used by PSP draws.
        void SetFixedFunctionVertices(std::vector<FixedFunctionVertex>&& vertices);

        /// Replaces the cached textured fixed-function vertex stream used by PSP draws.
        void SetFixedFunctionTexturedVertices(std::vector<FixedFunctionTexturedVertex>&& vertices);

        /// Returns whether the runtime model owns one non-empty untextured fixed-function vertex stream.
        bool HasFixedFunctionVertices() const;

        /// Returns whether the runtime model owns one non-empty textured fixed-function vertex stream.
        bool HasFixedFunctionTexturedVertices() const;

        /// Returns the number of cached untextured fixed-function vertices.
        int32_t GetFixedFunctionVertexCount() const;

        /// Returns the number of cached textured fixed-function vertices.
        int32_t GetFixedFunctionTexturedVertexCount() const;

        /// Returns the cached untextured fixed-function vertex stream.
        const FixedFunctionVertex* GetFixedFunctionVertices() const;

        /// Returns the cached textured fixed-function vertex stream.
        const FixedFunctionTexturedVertex* GetFixedFunctionTexturedVertices() const;

    private:
        /// Stores the ready-to-submit untextured fixed-function vertex stream for PSP draws.
        std::vector<FixedFunctionVertex> FixedFunctionVertices;

        /// Stores the ready-to-submit textured fixed-function vertex stream for PSP draws.
        std::vector<FixedFunctionTexturedVertex> FixedFunctionTexturedVertices;
    };
}
