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
    }
}
