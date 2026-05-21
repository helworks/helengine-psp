using System;

namespace helengine.psp.builder.tests {
    /// <summary>
    /// Guards the PSP 3D renderer source against invalid GU vertex-format usage.
    /// </summary>
    public sealed class PspRenderManager3DSourceTests {
        /// <summary>
        /// Ensures the renderer does not use the texture projection-map constant as a GU vertex-format flag.
        /// </summary>
        [Fact]
        public void Source_DoesNotUseGuNormalizedNormalAsVertexFormatFlag() {
            string sourcePath = Path.Combine(
                AppContext.BaseDirectory,
                "..",
                "..",
                "..",
                "..",
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(Path.GetFullPath(sourcePath));

            Assert.DoesNotContain("GU_NORMALIZED_NORMAL", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures the PSP 3D renderer does not delete top-level runtime assets inside the renderer release seam.
        /// </summary>
        [Fact]
        public void Source_DoesNotDeleteRuntimeModelOrMaterialInsideReleaseMethods() {
            string sourcePath = Path.Combine(
                AppContext.BaseDirectory,
                "..",
                "..",
                "..",
                "..",
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(Path.GetFullPath(sourcePath));

            Assert.DoesNotContain("delete static_cast<PspRuntimeModel*>(model);", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("delete static_cast<PspRuntimeMaterial*>(material);", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures PSP fixed-function lambert keeps the fast untextured path and uses the fixed-function textured path for textured meshes.
        /// </summary>
        [Fact]
        public void Source_FixedFunctionLambertKeepsFixedFunctionDrawablesForTexturedAndUntexturedMeshes() {
            string sourcePath = Path.Combine(
                AppContext.BaseDirectory,
                "..",
                "..",
                "..",
                "..",
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(Path.GetFullPath(sourcePath));

            Assert.Contains("SubmitFixedFunctionDrawable(\n                pspRuntimeModelData,", sourceContents, StringComparison.Ordinal);
            Assert.Contains("SubmitFixedFunctionTexturedDrawable(\n                    pspRuntimeModelData,", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("SubmitCpuLitTexturedDrawable(\n                    drawable,\n                    pspRuntimeModelData,", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures PSP textured draws resolve diffuse textures from the concrete drawable material instance instead of only from the root material.
        /// </summary>
        [Fact]
        public void Source_TexturedDrawsResolveTextureFromDrawableMaterialInstance() {
            string sourcePath = Path.Combine(
                AppContext.BaseDirectory,
                "..",
                "..",
                "..",
                "..",
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(Path.GetFullPath(sourcePath));

            Assert.Contains("PspRuntimeMaterial* pspRuntimeMaterial = static_cast<PspRuntimeMaterial*>(runtimeMaterial);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("PspRuntimeMaterial* rootPspRuntimeMaterial = static_cast<PspRuntimeMaterial*>(rootMaterial);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("const bool hasTexture = pspRuntimeMaterial->TryResolveTexture(texture);", sourceContents, StringComparison.Ordinal);
        }
    }
}
