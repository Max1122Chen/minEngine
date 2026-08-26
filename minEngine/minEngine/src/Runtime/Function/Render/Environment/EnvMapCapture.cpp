#include "EnvMapCapture.h"

#include "../EngineShaderUtils.h"
#include "../EnginePassUniforms.h"
#include "../TextureCubeLoader.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/EngineRHITextureUtils.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIPipelineLayout.h"
#include "Runtime/Function/Render/RHI/RHIRenderPass.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <algorithm>
#include <vector>

// Offline bake path: modern RHI only (CommandList / PSO / SRV / GenerateMips).

namespace minEngine
{
    namespace
    {
        constexpr float kCubeVertices[] = {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,

             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,

            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,

            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
        };

        const Vector3 kCaptureTargets[6] = {
            Vector3( 1.0f,  0.0f,  0.0f),
            Vector3(-1.0f,  0.0f,  0.0f),
            Vector3( 0.0f,  1.0f,  0.0f),
            Vector3( 0.0f, -1.0f,  0.0f),
            Vector3( 0.0f,  0.0f,  1.0f),
            Vector3( 0.0f,  0.0f, -1.0f),
        };

        const Vector3 kCaptureUps[6] = {
            Vector3(0.0f, -1.0f,  0.0f),
            Vector3(0.0f, -1.0f,  0.0f),
            Vector3(0.0f,  0.0f,  1.0f),
            Vector3(0.0f,  0.0f, -1.0f),
            Vector3(0.0f, -1.0f,  0.0f),
            Vector3(0.0f, -1.0f,  0.0f),
        };

        uint32_t ChannelsFromFormat(TextureFormat format)
        {
            if (format == TextureFormat::RGB16F)
            {
                return 3;
            }
            if (format == TextureFormat::RGBA16F)
            {
                return 4;
            }
            return 0;
        }

        struct EnvMapMeshResources
        {
            RHIBufferRef VertexBuffer;
            RHIVertexInputLayoutRef VertexLayout;
            uint32_t VertexCount = 0;
        };

        EnvMapMeshResources CreateEnvMapMesh(RHI& rhi)
        {
            RHICommandList cmdList(&rhi);
            EnvMapMeshResources resources;
            const uint32_t vertexByteSize = static_cast<uint32_t>(sizeof(kCubeVertices));
            resources.VertexCount = vertexByteSize / (3 * sizeof(float));

            RHIBufferCreateDesc vbDesc;
            vbDesc.Usage = RHIBufferUsage::Vertex;
            vbDesc.ByteSize = vertexByteSize;
            vbDesc.Stride = 3 * sizeof(float);
            vbDesc.ElementCount = resources.VertexCount;
            resources.VertexBuffer = cmdList.CreateBuffer(vbDesc, kCubeVertices);
            resources.VertexLayout = cmdList.CreateVertexInputLayout({
                {"a_Position", VertexElementType::Float3, false},
            });
            return resources;
        }

        void DrawEnvMapMesh(RHICommandList& cmdList, const EnvMapMeshResources& mesh)
        {
            cmdList.SetVertexBuffer(mesh.VertexBuffer.get());
            cmdList.Draw(mesh.VertexCount, 0);
        }

        void SubmitVulkanImmediateCommandsBeforeResourceDestroy(RHI& rhi)
        {
            if (RHIBackendSelection::IsVulkan())
            {
                rhi.RHIEndImmediateCommands();
            }
        }

        void TransitionTextureToShaderRead(RHICommandList& cmdList, RHITexture* texture)
        {
            if (texture == nullptr)
            {
                return;
            }

            RHITextureTransitionInfo transition{};
            transition.Texture = texture;
            cmdList.Transition(transition);
        }

        void BeginCubeFaceRenderPass(
            RHICommandList& cmdList,
            RHITexture* colorCube,
            uint32_t faceIndex,
            uint8_t mipIndex,
            RHITexture* depthTexture,
            uint32_t viewportSize)
        {
            RHIRenderPassInfo passInfo;
            passInfo.ColorAttachments[0].RenderTarget = colorCube;
            passInfo.ColorAttachments[0].ArraySlice = static_cast<int32_t>(faceIndex);
            passInfo.ColorAttachments[0].MipIndex = mipIndex;
            passInfo.ColorAttachments[0].Action = RHIRenderTargetActions::ClearStore;
            passInfo.DepthStencil.DepthStencilTarget = depthTexture;
            passInfo.DepthStencil.Action = RHIDepthStencilTargetActions::ClearDepthStencilStoreDepthStencil;
            passInfo.ClearValue.Depth = 1.0f;
            cmdList.BeginRenderPass(passInfo);
            // Bake cubemap faces without the scene-path Vulkan Y-flip; otherwise ±Y faces
            // end up rotated/split relative to samplerCube conventions.
            cmdList.SetViewport(0, 0, viewportSize, viewportSize, false);
        }

        struct EnvCaptureDrawResources
        {
            RHIGraphicsPipelineStateRef PipelineState;
            RHIPipelineLayoutRef PipelineLayout;
            RHIShaderBindingSetLayoutRef BindingLayout;
        };

        // Keep per-face UBOs + descriptor sets alive until the immediate CB has submitted.
        // Destroying them mid-loop (while still referenced by the recorded CB) causes DEVICE_LOST.
        struct EnvCapturePendingBindings
        {
            std::vector<RHIBufferRef> FrameUniformBuffers;
            std::vector<RHIShaderBindingSetRef> BindingSets;
        };

        EnvCaptureDrawResources CreateEnvCaptureDrawResources(
            RHICommandList& cmdList,
            RHIShader* shader,
            RHIVertexInputLayout* vertexLayout,
            uint32_t sourceTextureUnit)
        {
            EnvCaptureDrawResources resources;

            resources.BindingLayout = cmdList.CreateShaderBindingSetLayout({
                {EngineShaderBindings::kEnvCapture_SourceSRV,
                 RHIShaderBindingType::TextureSRV,
                 sourceTextureUnit,
                 RHIGraphicsShaderStage::Pixel},
                {EngineShaderBindings::kEnvCapture_FrameData,
                 RHIShaderBindingType::UniformBuffer,
                 EngineShaderBindings::kGL_EnvCaptureFrameDataUBO,
                 RHIGraphicsShaderStage::Vertex},
            });
            resources.PipelineLayout = cmdList.CreatePipelineLayout({resources.BindingLayout.get()});

            RHIGraphicsPSODesc psoDesc;
            psoDesc.PipelineLayout = resources.PipelineLayout.get();
            psoDesc.VertexShader = shader;
            psoDesc.PixelShader = shader;
            psoDesc.VertexInputLayout = vertexLayout;
            psoDesc.DepthStencilState.bDepthTestEnabled = true;
            psoDesc.DepthStencilState.bDepthWriteEnabled = true;
            psoDesc.BlendState.bBlendEnabled = false;
            resources.PipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);
            return resources;
        }

        RHIShaderBindingSet* AppendEnvCaptureBindingSet(
            RHICommandList& cmdList,
            EnvCaptureDrawResources& drawResources,
            EnvCapturePendingBindings& pending,
            RHIShaderResourceView* sourceSrv,
            const EnvCaptureFrameUBO& frameData)
        {
            if (!drawResources.BindingLayout || sourceSrv == nullptr)
            {
                return nullptr;
            }

            RHIBufferCreateDesc frameDesc;
            frameDesc.Usage = RHIBufferUsage::Uniform;
            frameDesc.ByteSize = sizeof(EnvCaptureFrameUBO);
            RHIBufferRef frameUniformBuffer = cmdList.CreateBuffer(frameDesc, &frameData);
            if (!frameUniformBuffer)
            {
                return nullptr;
            }

            RHIShaderBindingSetRef bindingSet = cmdList.CreateShaderBindingSet(
                drawResources.BindingLayout.get(),
                {
                    {RHIShaderBindingType::TextureSRV, nullptr, sourceSrv},
                    {RHIShaderBindingType::UniformBuffer, frameUniformBuffer.get(), nullptr},
                });
            if (!bindingSet)
            {
                return nullptr;
            }

            pending.FrameUniformBuffers.push_back(std::move(frameUniformBuffer));
            pending.BindingSets.push_back(std::move(bindingSet));
            return pending.BindingSets.back().get();
        }

        RHIShaderResourceViewRef CreateSourceSRV(RHICommandList& cmdList, RHITexture* sourceTexture)
        {
            RHITextureSRVDesc srvDesc;
            srvDesc.Texture = sourceTexture;
            return cmdList.CreateShaderResourceView(srvDesc);
        }
    }

    std::shared_ptr<TextureCube> EnvMapCapture::EquirectToCubemap(
        RHI& rhi,
        RHITexture& equirectTexture,
        const std::filesystem::path& engineDefaultAssetsRoot,
        uint32_t faceSize,
        std::string* outError)
    {
        auto reportError = [&](const std::string& message) -> std::shared_ptr<TextureCube>
        {
            ME_CORE_ERROR("EnvMapCapture: {}", message);
            if (outError)
            {
                *outError = message;
            }
            SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
            return nullptr;
        };

        if (faceSize == 0)
        {
            return reportError("faceSize must be > 0.");
        }

        const TextureFormat cubeFormat = TextureFormat::RGB16F;
        const uint32_t channels = ChannelsFromFormat(cubeFormat);

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        std::shared_ptr<RHIShader> captureShader = EngineShaderUtils::CreateShaderFromSpirvFiles(
            rhi,
            shaderDirectory / "equirect_to_cubemap.vert",
            shaderDirectory / "equirect_to_cubemap.frag",
            outError);
        if (!captureShader || !captureShader->IsValid())
        {
            return reportError("failed to load equirect_to_cubemap SPIR-V shader.");
        }

        const uint32_t environmentMipCount =
            RHIBackendSelection::IsVulkan() ? 1u : ComputeTextureMipCount(faceSize);
        RHITextureRef environmentCube =
            TextureCubeLoader::CreateRenderTargetCube(rhi, faceSize, cubeFormat, environmentMipCount);
        if (!environmentCube)
        {
            return reportError("failed to allocate environment cubemap.");
        }

        RHITextureRef depthTarget = rhi.RHICreateTexture2D(MakeDepthTextureDesc(faceSize, faceSize), nullptr);
        if (!depthTarget)
        {
            return reportError("failed to allocate capture depth texture.");
        }

        const EnvMapMeshResources mesh = CreateEnvMapMesh(rhi);
        RHICommandList cmdList(&rhi);
        RHIShaderResourceViewRef sourceSrv = CreateSourceSRV(cmdList, &equirectTexture);
        EnvCaptureDrawResources drawResources = CreateEnvCaptureDrawResources(
            cmdList,
            captureShader.get(),
            mesh.VertexLayout.get(),
            EngineShaderBindings::kGL_EnvCaptureSourceUnit);
        EnvCapturePendingBindings pendingBindings;

        const Matrix4 captureProjection = RHIBackendSelection::IsVulkan()
            ? glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 0.1f, 10.0f)
            : glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            const Matrix4 captureView = glm::lookAt(
                Vector3(0.0f),
                kCaptureTargets[faceIndex],
                kCaptureUps[faceIndex]);

            BeginCubeFaceRenderPass(
                cmdList,
                environmentCube.get(),
                faceIndex,
                0,
                depthTarget.get(),
                faceSize);

            EnvCaptureFrameUBO frameData{};
            frameData.Projection = captureProjection;
            frameData.View = captureView;
            cmdList.SetGraphicsPipelineState(drawResources.PipelineState.get());
            if (RHIShaderBindingSet* bindingSet = AppendEnvCaptureBindingSet(
                    cmdList, drawResources, pendingBindings, sourceSrv.get(), frameData))
            {
                cmdList.SetShaderBindingSet(EngineShaderBindings::kSetEnvCapture, bindingSet);
            }
            DrawEnvMapMesh(cmdList, mesh);
            cmdList.EndRenderPass();
        }

        TransitionTextureToShaderRead(cmdList, environmentCube.get());
        cmdList.GenerateMips(environmentCube.get());

        auto environment = TextureCubeLoader::WrapTextureCube(std::move(environmentCube), faceSize, channels);
        SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
        return environment;
    }

    std::shared_ptr<TextureCube> EnvMapCapture::ConvolveIrradiance(
        RHI& rhi,
        RHITexture& environmentCube,
        const std::filesystem::path& engineDefaultAssetsRoot,
        uint32_t faceSize,
        std::string* outError)
    {
        auto reportError = [&](const std::string& message) -> std::shared_ptr<TextureCube>
        {
            ME_CORE_ERROR("EnvMapCapture: {}", message);
            if (outError)
            {
                *outError = message;
            }
            SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
            return nullptr;
        };

        if (faceSize == 0)
        {
            return reportError("irradiance faceSize must be > 0.");
        }

        const TextureFormat cubeFormat = environmentCube.GetDesc().Format;
        const uint32_t channels = ChannelsFromFormat(cubeFormat);
        if (channels == 0)
        {
            return reportError("environment cubemap must be RGB16F or RGBA16F.");
        }

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        std::shared_ptr<RHIShader> irradianceShader = EngineShaderUtils::CreateShaderFromSpirvFiles(
            rhi,
            shaderDirectory / "irradiance_convolution.vert",
            shaderDirectory / "irradiance_convolution.frag",
            outError);
        if (!irradianceShader || !irradianceShader->IsValid())
        {
            return reportError("failed to load irradiance convolution SPIR-V shader.");
        }

        RHITextureRef irradianceCube =
            TextureCubeLoader::CreateRenderTargetCube(rhi, faceSize, cubeFormat);
        if (!irradianceCube)
        {
            return reportError("failed to allocate irradiance cubemap.");
        }

        RHITextureRef depthTarget = rhi.RHICreateTexture2D(MakeDepthTextureDesc(faceSize, faceSize), nullptr);
        if (!depthTarget)
        {
            return reportError("failed to allocate capture depth texture.");
        }

        const EnvMapMeshResources mesh = CreateEnvMapMesh(rhi);
        RHICommandList cmdList(&rhi);
        RHIShaderResourceViewRef sourceSrv = CreateSourceSRV(cmdList, &environmentCube);
        EnvCaptureDrawResources drawResources = CreateEnvCaptureDrawResources(
            cmdList,
            irradianceShader.get(),
            mesh.VertexLayout.get(),
            EngineShaderBindings::kGL_EnvCaptureSourceUnit);
        EnvCapturePendingBindings pendingBindings;

        const Matrix4 captureProjection = RHIBackendSelection::IsVulkan()
            ? glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 0.1f, 10.0f)
            : glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            const Matrix4 captureView = glm::lookAt(
                Vector3(0.0f),
                kCaptureTargets[faceIndex],
                kCaptureUps[faceIndex]);

            BeginCubeFaceRenderPass(
                cmdList,
                irradianceCube.get(),
                faceIndex,
                0,
                depthTarget.get(),
                faceSize);

            EnvCaptureFrameUBO frameData{};
            frameData.Projection = captureProjection;
            frameData.View = captureView;
            cmdList.SetGraphicsPipelineState(drawResources.PipelineState.get());
            if (RHIShaderBindingSet* bindingSet = AppendEnvCaptureBindingSet(
                    cmdList, drawResources, pendingBindings, sourceSrv.get(), frameData))
            {
                cmdList.SetShaderBindingSet(EngineShaderBindings::kSetEnvCapture, bindingSet);
            }
            DrawEnvMapMesh(cmdList, mesh);
            cmdList.EndRenderPass();
        }

        TransitionTextureToShaderRead(cmdList, irradianceCube.get());

        ME_CORE_INFO(
            "EnvMapCapture: convolved irradiance cubemap {}x{} from environment.",
            faceSize,
            faceSize);

        auto irradiance = TextureCubeLoader::WrapTextureCube(std::move(irradianceCube), faceSize, channels);
        SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
        return irradiance;
    }

    std::shared_ptr<TextureCube> EnvMapCapture::PrefilterEnvironment(
        RHI& rhi,
        RHITexture& environmentCube,
        const std::filesystem::path& engineDefaultAssetsRoot,
        uint32_t faceSize,
        std::string* outError)
    {
        auto reportError = [&](const std::string& message) -> std::shared_ptr<TextureCube>
        {
            ME_CORE_ERROR("EnvMapCapture: {}", message);
            if (outError)
            {
                *outError = message;
            }
            SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
            return nullptr;
        };

        if (faceSize == 0)
        {
            return reportError("prefilter faceSize must be > 0.");
        }

        const TextureFormat cubeFormat = environmentCube.GetDesc().Format;
        const uint32_t channels = ChannelsFromFormat(cubeFormat);
        if (channels == 0)
        {
            return reportError("environment cubemap must be RGB16F or RGBA16F.");
        }

        const uint32_t environmentResolution = environmentCube.GetDesc().Width;
        if (environmentResolution == 0)
        {
            return reportError("environment cubemap has zero width.");
        }

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        std::shared_ptr<RHIShader> prefilterShader = EngineShaderUtils::CreateShaderFromSpirvFiles(
            rhi,
            shaderDirectory / "prefilter.vert",
            shaderDirectory / "prefilter.frag",
            outError);
        if (!prefilterShader || !prefilterShader->IsValid())
        {
            return reportError("failed to load prefilter SPIR-V shader.");
        }

        const uint32_t maxMipLevel = kMaterialPBRMaxReflectionLod;
        RHITextureRef prefilterCube =
            TextureCubeLoader::CreateRenderTargetCube(rhi, faceSize, cubeFormat, maxMipLevel + 1);
        if (!prefilterCube)
        {
            return reportError("failed to allocate prefilter cubemap.");
        }

        RHITextureRef depthTarget = rhi.RHICreateTexture2D(MakeDepthTextureDesc(faceSize, faceSize), nullptr);
        if (!depthTarget)
        {
            return reportError("failed to allocate capture depth texture.");
        }

        const EnvMapMeshResources mesh = CreateEnvMapMesh(rhi);
        RHICommandList cmdList(&rhi);
        RHIShaderResourceViewRef sourceSrv = CreateSourceSRV(cmdList, &environmentCube);
        EnvCaptureDrawResources drawResources = CreateEnvCaptureDrawResources(
            cmdList,
            prefilterShader.get(),
            mesh.VertexLayout.get(),
            EngineShaderBindings::kGL_EnvCaptureSourceUnit);
        EnvCapturePendingBindings pendingBindings;

        const Matrix4 captureProjection = RHIBackendSelection::IsVulkan()
            ? glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 0.1f, 10.0f)
            : glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        for (uint32_t mip = 0; mip <= maxMipLevel; ++mip)
        {
            const uint32_t mipWidth = std::max(1u, faceSize >> mip);
            const float roughness =
                maxMipLevel > 0 ? static_cast<float>(mip) / static_cast<float>(maxMipLevel) : 0.0f;

            for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
            {
                const Matrix4 captureView = glm::lookAt(
                    Vector3(0.0f),
                    kCaptureTargets[faceIndex],
                    kCaptureUps[faceIndex]);

                BeginCubeFaceRenderPass(
                    cmdList,
                    prefilterCube.get(),
                    faceIndex,
                    static_cast<uint8_t>(mip),
                    depthTarget.get(),
                    mipWidth);

                EnvCaptureFrameUBO frameData{};
                frameData.Projection = captureProjection;
                frameData.View = captureView;
                frameData.Roughness = roughness;
                frameData.EnvironmentResolution = static_cast<float>(environmentResolution);
                cmdList.SetGraphicsPipelineState(drawResources.PipelineState.get());
                if (RHIShaderBindingSet* bindingSet = AppendEnvCaptureBindingSet(
                        cmdList, drawResources, pendingBindings, sourceSrv.get(), frameData))
                {
                    cmdList.SetShaderBindingSet(EngineShaderBindings::kSetEnvCapture, bindingSet);
                }
                DrawEnvMapMesh(cmdList, mesh);
                cmdList.EndRenderPass();
            }
        }

        TransitionTextureToShaderRead(cmdList, prefilterCube.get());

        ME_CORE_INFO(
            "EnvMapCapture: prefiltered environment cubemap {}x{} ({} mips) from environment.",
            faceSize,
            faceSize,
            PrefilterMipLevelCount());

        auto prefilter = TextureCubeLoader::WrapTextureCube(std::move(prefilterCube), faceSize, channels);
        SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
        return prefilter;
    }
}
