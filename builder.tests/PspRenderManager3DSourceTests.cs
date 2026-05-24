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
            Assert.Contains("PspRuntimeMaterial* rootPspRuntimeMaterial = static_cast<PspRuntimeMaterial*>(rootMaterial);", sourceContents, StringComparison.Ordinal);
            Assert.Contains("const bool hasTexture = pspRuntimeMaterial->TryResolveTexture(texture);", sourceContents, StringComparison.Ordinal);
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
        /// Ensures the PSP 3D renderer and runtime material follow the shared shader-material contract expected by generated core.
        /// </summary>
        [Fact]
        public void Source_RendererAndRuntimeMaterialUseShaderMaterialAssetContract() {
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

            Assert.Contains("RuntimeMaterial* BuildMaterialFromRaw(ShaderMaterialAsset* materialAsset, ShaderAsset* shaderAsset) override;", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.DoesNotContain("RuntimeMaterial* BuildMaterialFromRaw(MaterialAsset* materialAsset, ShaderAsset* shaderAsset) override;", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.Contains("void LoadFromCooked(ShaderMaterialAsset* materialAsset);", runtimeMaterialHeaderContents, StringComparison.Ordinal);
            Assert.Contains("void PspRuntimeMaterial::LoadFromCooked(ShaderMaterialAsset* materialAsset)", runtimeMaterialSourceContents, StringComparison.Ordinal);
            Assert.DoesNotContain("void LoadFromCooked(MaterialAsset* materialAsset);", runtimeMaterialHeaderContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures the PSP renderer implements the generated shader-material runtime interface expected by packaged material loading.
        /// </summary>
        [Fact]
        public void Source_RendererImplementsShaderRuntimeInterface() {
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

            Assert.Contains("public IShaderRenderManager3D", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.Contains("ShaderCompileTarget get_ShaderCompileTarget() override;", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.Contains("void InvalidateShaderResources(std::string shaderAssetId, ShaderAsset* shaderAsset) override;", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.Contains("ShaderCompileTarget PspRenderManager3D::get_ShaderCompileTarget()", renderManagerSourceContents, StringComparison.Ordinal);
            Assert.Contains("return ShaderCompileTarget::DirectX11;", renderManagerSourceContents, StringComparison.Ordinal);
            Assert.Contains("void PspRenderManager3D::InvalidateShaderResources(std::string shaderAssetId, ShaderAsset* shaderAsset)", renderManagerSourceContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures the PSP runtime material stays shader-backed so it can use the shared layout and property-block helpers.
        /// </summary>
        [Fact]
        public void Source_RuntimeMaterialInheritsShaderRuntimeMaterialContract() {
            string repositoryRootPath = PspRepositoryPathResolver.ResolveRepositoryRootPath();
            string runtimeMaterialHeaderPath = Path.Combine(
                repositoryRootPath,
                "src",
                "platform",
                "psp",
                "rendering",
                "PspRuntimeMaterial.hpp");

            string runtimeMaterialHeaderContents = File.ReadAllText(runtimeMaterialHeaderPath);

            Assert.Contains("class PspRuntimeMaterial final : public ShaderRuntimeMaterial", runtimeMaterialHeaderContents, StringComparison.Ordinal);
            Assert.DoesNotContain("class PspRuntimeMaterial final : public RuntimeMaterial", runtimeMaterialHeaderContents, StringComparison.Ordinal);
        }

        /// <summary>
        /// Ensures the PSP renderer overrides the packaged raw-material entrypoint used by runtime scene loading.
        /// </summary>
        [Fact]
        public void Source_RendererOverridesBuildMaterialFromRawAssetForPackagedSceneLoading() {
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

            Assert.Contains("RuntimeMaterial* BuildMaterialFromRawAsset(ContentManager* assetContentManager, std::string contentRootPath, std::string materialAssetPath) override;", renderManagerHeaderContents, StringComparison.Ordinal);
            Assert.Contains("ShaderRuntimeMaterialLoader::BuildMaterialFromRawAsset(this, assetContentManager, contentRootPath, materialAssetPath);", renderManagerSourceContents, StringComparison.Ordinal);
        }
    }
}
