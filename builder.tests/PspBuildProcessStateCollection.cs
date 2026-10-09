namespace helengine.psp.builder.tests;

/// <summary>Serializes tests that temporarily change the process working directory or PSP repository override.</summary>
[CollectionDefinition(nameof(PspBuildProcessStateCollection), DisableParallelization = true)]
public sealed class PspBuildProcessStateCollection {
}
