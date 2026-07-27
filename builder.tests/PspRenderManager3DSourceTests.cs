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
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.DoesNotContain("GU_NORMALIZED_NORMAL", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures the PSP 3D renderer does not delete top-level runtime assets inside the renderer release seam.
        /// </summary>
        [Fact]
        public void Source_DoesNotDeleteRuntimeModelOrMaterialInsideReleaseMethods() {
            string sourcePath = Path.Combine(
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.DoesNotContain("delete static_cast<PspRuntimeModel*>(model);", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("delete static_cast<PspRuntimeMaterial*>(material);", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures PSP fixed-function lambert keeps the fast untextured path and uses the fixed-function textured path for textured meshes.
        /// </summary>
        [Fact]
        public void Source_FixedFunctionLambertKeepsFixedFunctionDrawablesForTexturedAndUntexturedMeshes() {
            string sourcePath = Path.Combine(
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

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
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("PspRuntimeMaterial* pspRuntimeMaterial = static_cast<PspRuntimeMaterial*>(runtimeMaterial);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("const bool hasTexture = pspRuntimeMaterial->TryResolveTexture(texture);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("const float4& baseColor = pspRuntimeMaterial->GetBaseColor();", sourceContents, StringComparison.Ordinal);
            Assert.Contains("const bool useLighting = UsesDirectionalLighting(pspRuntimeMaterial);", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("const float4& baseColor = rootPspRuntimeMaterial->GetBaseColor();", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("const bool useLighting = UsesDirectionalLighting(rootPspRuntimeMaterial);", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures PSP textured 3D draws preserve the engine's normalized UVs instead of multiplying them by texture dimensions.
        /// </summary>
        [Fact]
        public void Source_TexturedDrawsPreserveNormalizedUvs() {
            string sourcePath = Path.Combine(
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.DoesNotContain("ConvertNormalizedTextureCoordinatesToPspTexels", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("normalizedTextureCoordinates.X * texture->get_Width()", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("normalizedTextureCoordinates.Y * texture->get_Height()", sourceContents, StringComparison.Ordinal);
            Assert.Contains("sourceVertex.U,", sourceContents, StringComparison.Ordinal);
            Assert.Contains("sourceVertex.V,", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures PSP textured 3D draws explicitly restore GU UV mapping state so 3D texture coordinates cannot inherit stale mapping or scale state.
        /// </summary>
        [Fact]
        public void Source_TexturedDrawsResetGuTextureCoordinateState() {
            string sourcePath = Path.Combine(
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("sceGuTexProjMapMode(GU_UV);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("sceGuTexScale(1.0f, 1.0f);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("sceGuTexOffset(0.0f, 0.0f);", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures PSP baked mesh variants omit entity scale at render time while retaining the normal fixed-function path for other meshes.
        /// </summary>
        [Fact]
        public void Source_BakedMeshScaleUsesSyntheticMarkerAndOmitsModelScale() {
            string sourcePath = Path.Combine(
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("GetSyntheticBooleanMemberOrDefault(\"MeshBakeScale\", false)", sourceContents, StringComparison.Ordinal);
            Assert.Contains("(usesBakedScale || useScaledGpuVertices)", sourceContents, StringComparison.Ordinal);
            Assert.Contains("? BuildWorldMatrixWithoutScale(drawableParent)", sourceContents, StringComparison.Ordinal);
            Assert.Contains("runtimeModel->SetRawModelAsset(modelAsset);", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures PSP camera projections honor authored clip planes instead of applying one global far-plane distance.
        /// </summary>
        [Fact]
        public void Source_RenderCameraUsesAuthoredClipPlanes() {
            string sourcePath = Path.Combine(
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("const float nearPlaneDistance = camera->get_NearPlaneDistance();", sourceContents, StringComparison.Ordinal);
            Assert.Contains("const float farPlaneDistance = camera->get_FarPlaneDistance();", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("constexpr float FarPlaneDistance = 100.0f;", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures PSP directional lighting uses a valid GE light component mode.
        /// </summary>
        [Fact]
        public void Source_FixedFunctionDirectionalLightUsesAmbientAndDiffuseLightComponentMode() {
            string sourcePath = Path.Combine(
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("sceGuLight(0, GU_DIRECTIONAL, GU_AMBIENT_AND_DIFFUSE, &lightVector);", sourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("sceGuLight(0, GU_DIRECTIONAL, GU_DIFFUSE, &lightVector);", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures generated model assets with empty GUID ids do not collide in the PSP runtime model cache.
        /// </summary>
        [Fact]
        public void Source_ModelCacheDoesNotReuseEmptyGuidModelIds() {
            string sourcePath = Path.Combine(
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("bool IsCacheableModelId(const std::string& modelId)", sourceContents, StringComparison.Ordinal);
            Assert.Contains("modelId != \"00000000-0000-0000-0000-000000000000\";", sourceContents, StringComparison.Ordinal);
            Assert.Contains("if (data != nullptr && IsCacheableModelId(data->get_Id()))", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures each PSP 3D camera pass restores frustum clipping and a full-frame scissor before drawing large world triangles.
        /// </summary>
        [Fact]
        public void Source_RenderCameraRestoresClipPlanesAndFullFrameScissor() {
            string sourcePath = Path.Combine(
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("sceGuEnable(GU_CLIP_PLANES);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("sceGuEnable(GU_SCISSOR_TEST);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("sceGuScissor(0, 0, mainWindowSize.X, mainWindowSize.Y);", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures PSP 3D applies cooked material double-sidedness instead of disabling back-face culling for every mesh.
        /// </summary>
        [Fact]
        public void Source_AppliesCookedMaterialDoubleSidednessToGuCulling() {
            string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
            string renderSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "rendering", "PspRenderManager3D.cpp");
            string materialHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "rendering", "PspRuntimeMaterial.hpp");
            string materialSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "psp", "rendering", "PspRuntimeMaterial.cpp");
            string renderSource = File.ReadAllText(renderSourcePath);
            string materialHeader = File.ReadAllText(materialHeaderPath);
            string materialSource = File.ReadAllText(materialSourcePath);

            Assert.Contains("bool IsDoubleSided() const;", materialHeader, StringComparison.Ordinal);
            Assert.Contains("DoubleSided = materialAsset->DoubleSided;", materialSource, StringComparison.Ordinal);
            Assert.Contains("sceGuEnable(GU_CULL_FACE);", renderSource, StringComparison.Ordinal);
            Assert.Contains("sceGuDisable(GU_CULL_FACE);", renderSource, StringComparison.Ordinal);
            Assert.Contains("pspRuntimeMaterial->IsDoubleSided()", renderSource, StringComparison.Ordinal);
        }

        /// Ensures PSP GU matrix uploads preserve the generated row-major field order in the native upload buffer.
        /// </summary>
        [Fact]
        public void Source_CreatePspMatrixBufferPreservesGeneratedMatrixFieldOrder() {
            string sourcePath = Path.Combine(
                PspRepositoryPathResolver.ResolveRepositoryRootPath(),
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");
            string sourceContents = File.ReadAllText(sourcePath);

            Assert.Contains("buffer.M[0][1] = matrix.M12;", sourceContents, StringComparison.Ordinal);
            Assert.Contains("buffer.M[1][0] = matrix.M21;", sourceContents, StringComparison.Ordinal);
            Assert.Contains("buffer.M[2][3] = matrix.M34;", sourceContents, StringComparison.Ordinal);
            Assert.Contains("buffer.M[3][2] = matrix.M43;", sourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures the PSP 3D renderer and runtime material follow the shaderless fixed-function material contract expected by generated core.
        /// </summary>
        [Fact]
        public void Source_RendererAndRuntimeMaterialUsePlatformMaterialAssetContract() {
            string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
            string renderManagerHeaderPath = Path.Combine(
                repositoryRootPath,
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.hpp");
            string runtimeMaterialHeaderPath = Path.Combine(
                repositoryRootPath,
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRuntimeMaterial.hpp");
            string runtimeMaterialSourcePath = Path.Combine(
                repositoryRootPath,
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRuntimeMaterial.cpp");

            string renderManagerHeaderContents = File.ReadAllText(renderManagerHeaderPath);
            string runtimeMaterialHeaderContents = File.ReadAllText(runtimeMaterialHeaderPath);
            string runtimeMaterialSourceContents = File.ReadAllText(runtimeMaterialSourcePath);

            Assert.Contains("RuntimeMaterial* BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset) override;", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.Contains("RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override;", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.Contains("void LoadFromCooked(PlatformMaterialAsset* materialAsset);", runtimeMaterialHeaderContents, StringComparison.Ordinal);
            Assert.Contains("void PspRuntimeMaterial::LoadFromCooked(PlatformMaterialAsset* materialAsset)", runtimeMaterialSourceContents, StringComparison.Ordinal);
            Assert.Contains("const PspRuntimeMaterial* GetParentPspRuntimeMaterial() const;", runtimeMaterialHeaderContents, StringComparison.Ordinal);
            Assert.Contains("if (HasAuthoredBaseColor) {", runtimeMaterialSourceContents, StringComparison.Ordinal);
            Assert.Contains("if (HasAuthoredLightingConfiguration) {", runtimeMaterialSourceContents, StringComparison.Ordinal);
            Assert.Contains("const PspRuntimeMaterial* parentMaterial = GetParentPspRuntimeMaterial();", runtimeMaterialSourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("ShaderMaterialAsset", runtimeMaterialHeaderContents, StringComparison.Ordinal);
            Assert.DoesNotContain("ShaderRuntimeMaterial", runtimeMaterialHeaderContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures the PSP renderer excludes shader-runtime interfaces after fixed-function material cooking.
        /// </summary>
        [Fact]
        public void Source_RendererExcludesShaderRuntimeInterface() {
            string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
            string renderManagerHeaderPath = Path.Combine(
                repositoryRootPath,
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.hpp");
            string renderManagerSourcePath = Path.Combine(
                repositoryRootPath,
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");

            string renderManagerHeaderContents = File.ReadAllText(renderManagerHeaderPath);
            string renderManagerSourceContents = File.ReadAllText(renderManagerSourcePath);

            Assert.DoesNotContain("IShaderRenderManager3D", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.DoesNotContain("ShaderCompileTarget", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.DoesNotContain("ShaderAsset", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.DoesNotContain("ShaderRuntimeMaterialLoader", renderManagerSourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("MaterialLayoutBuilder", renderManagerSourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures the PSP runtime material uses the generic fixed-function runtime-material contract.
        /// </summary>
        [Fact]
        public void Source_RuntimeMaterialInheritsFixedFunctionRuntimeMaterialContract() {
            string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
            string runtimeMaterialHeaderPath = Path.Combine(
                repositoryRootPath,
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRuntimeMaterial.hpp");

            string runtimeMaterialHeaderContents = File.ReadAllText(runtimeMaterialHeaderPath);

            Assert.Contains("class PspRuntimeMaterial final : public RuntimeMaterial", runtimeMaterialHeaderContents, StringComparison.Ordinal);
            Assert.DoesNotContain("class PspRuntimeMaterial final : public ShaderRuntimeMaterial", runtimeMaterialHeaderContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures the PSP renderer overrides the packaged cooked-material entrypoint used by runtime scene loading.
        /// </summary>
        [Fact]
        public void Source_RendererOverridesBuildMaterialFromCookedForPackagedSceneLoading() {
            string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
            string renderManagerHeaderPath = Path.Combine(
                repositoryRootPath,
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.hpp");
            string renderManagerSourcePath = Path.Combine(
                repositoryRootPath,
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRenderManager3D.cpp");

            string renderManagerHeaderContents = File.ReadAllText(renderManagerHeaderPath);
            string renderManagerSourceContents = File.ReadAllText(renderManagerSourcePath);

            Assert.Contains("RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override;", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.Contains("Stream* stream = contentStreamSource->OpenRead(cookedAssetPath);", renderManagerSourceContents, StringComparison.Ordinal);
            Assert.Contains("AssetSerializer::Deserialize(stream)", renderManagerSourceContents, StringComparison.Ordinal);
        }
    }
}
