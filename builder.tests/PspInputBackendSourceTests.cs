using System;

namespace helengine.psp.builder.tests {
    /// <summary>
    /// Guards the PSP input backend source against per-frame native gamepad-array allocations.
    /// </summary>
    public sealed class PspInputBackendSourceTests {
        /// <summary>
        /// Ensures the PSP input backend preallocates alternating gamepad buffers instead of allocating one new array every captured frame.
        /// </summary>
        [Fact]
        public void Source_CaptureFrame_reuses_preallocated_gamepad_buffers() {
            string sourcePath = Path.Combine(
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "PspInputBackend.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("new Array<InputGamepadState>(1)", sourceContents, StringComparison.Ordinal);
            Assert.Contains("GamepadBuffers[ActiveGamepadBufferIndex]", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("Array<InputGamepadState>* gamepads = new Array<InputGamepadState>(1);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("ActiveGamepadBufferIndex = (ActiveGamepadBufferIndex + 1) % 2;", sourceContents, StringComparison.Ordinal);
        }
    }
}
