#include "SkyBoxPass.h"

#include "Render/DrawCommands/MeshDrawPacket.h"
#include "Render/RenderGraph/RenderGraphFrameResources.h"
#include "Render/RenderGraph/RenderPassBuilder.h"
#include "Render/RenderGraph/SceneRenderPassUtils.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"
#include "Runtime/Function/Render/SceneRenderContext.h"

#include "../../RenderCamera.h"
#include "../../EngineShaderUtils.h"
#include "../../EnginePassUniforms.h"
#include "../../SkyBoxSceneProxies/SkyBoxSceneProxy.h"
#include "../../TextureCubeLoader.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/Texture.h"

#include <array>

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

        std::shared_ptr<TextureCube> LoadSkyEnvironmentCube(RHI& rhi, const std::filesystem::path& engineDefaultAssetsRoot)
        {
            const std::string iblDirectory = (engineDefaultAssetsRoot / "Textures" / "IBL").string();
            std::string error;
            if (std::shared_ptr<TextureCube> environment = TextureCubeLoader::LoadCubeMapFromDirectory(
                    rhi,
                    iblDirectory,
                    "environment",
                    true,
                    &error))
            {
                return environment;
            }

            ME_CORE_WARN(
                "SkyBoxPass: could not load environment cubemap from {} ({}); using validation cube.",
                iblDirectory,
                error);

            const std::array<uint8_t, 4> faceColors[6] = {
                std::array<uint8_t, 4>{ 64, 128, 255, 255 },
                std::array<uint8_t, 4>{ 32, 64, 128, 255 },
                std::array<uint8_t, 4>{ 128, 192, 255, 255 },
                std::array<uint8_t, 4>{ 16, 32, 64, 255 },
                std::array<uint8_t, 4>{ 96, 160, 224, 255 },
                std::array<uint8_t, 4>{ 48, 96, 160, 255 },
            };
            return TextureCubeLoader::CreateSolidColorCube(rhi, 32, faceColors, &error);
        }
    }

    void SkyBoxPass::Initialize(RHI& rhi, const std::filesystem::path& engineDefaultAssetsRoot)
    {
        Shutdown();

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        m_SkyShader = EngineShaderUtils::CreateShaderFromFiles(
            rhi,
            shaderDirectory / "background.vert",
            shaderDirectory / "background.frag");
        if (!m_SkyShader || !m_SkyShader->IsValid())
        {
            ME_CORE_ERROR("SkyBoxPass: failed to compile background shader.");
            return;
        }

        RHICommandList cmdList(&rhi);

        const uint32_t vertexByteSize = static_cast<uint32_t>(sizeof(kCubeVertices));
        const uint32_t vertexCount = vertexByteSize / (3 * sizeof(float));
        RHIBufferCreateDesc vbDesc;
        vbDesc.Usage = RHIBufferUsage::Vertex;
        vbDesc.ByteSize = vertexByteSize;
        vbDesc.Stride = 3 * sizeof(float);
        vbDesc.ElementCount = vertexCount;
        m_CubeVertexBuffer = cmdList.CreateBuffer(vbDesc, kCubeVertices);
        m_CubeVertexLayout = cmdList.CreateVertexInputLayout({
            {"a_Position", VertexElementType::Float3, false},
        });

        m_SkyShaderBindingSetLayout = cmdList.CreateShaderBindingSetLayout({
            {EngineShaderBindings::kSkyPass_EnvironmentSRV,
             RHIShaderBindingType::TextureSRV,
             EngineShaderBindings::kGL_SkyEnvironmentUnit,
             RHIGraphicsShaderStage::Pixel},
            {EngineShaderBindings::kSkyPass_FrameData,
             RHIShaderBindingType::UniformBuffer,
             EngineShaderBindings::kGL_SkyFrameDataUBO,
             RHIGraphicsShaderStage::Vertex},
        });

        m_SkyPipelineLayout = cmdList.CreatePipelineLayout({m_SkyShaderBindingSetLayout.get()});

        RHIBufferCreateDesc frameDesc;
        frameDesc.Usage = RHIBufferUsage::Uniform;
        frameDesc.ByteSize = sizeof(SkyPassFrameUBO);
        m_SkyFrameUniformBuffer = cmdList.CreateBuffer(frameDesc, nullptr);

        RHIGraphicsPSODesc psoDesc;
        psoDesc.PipelineLayout = m_SkyPipelineLayout.get();
        psoDesc.VertexShader = m_SkyShader.get();
        psoDesc.PixelShader = m_SkyShader.get();
        psoDesc.VertexInputLayout = m_CubeVertexLayout.get();
        psoDesc.DepthStencilState.bDepthTestEnabled = true;
        psoDesc.DepthStencilState.bDepthWriteEnabled = false;
        psoDesc.DepthStencilState.DepthCompare = RHIDepthCompareFunc::LessEqual;
        m_SkyPipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);

        m_EnvironmentCube = LoadSkyEnvironmentCube(rhi, engineDefaultAssetsRoot);
        if (!m_EnvironmentCube)
        {
            ME_CORE_ERROR("SkyBoxPass: failed to create sky environment cubemap.");
            return;
        }

        RHITexture* environmentTexture = m_EnvironmentCube->GetRHITexture();
        if (!environmentTexture)
        {
            ME_CORE_ERROR("SkyBoxPass: environment cubemap has no RHI texture.");
            return;
        }

        RHITextureSRVDesc srvDesc;
        srvDesc.Texture = environmentTexture;
        m_EnvironmentSRV = cmdList.CreateShaderResourceView(srvDesc);
        m_SkyShaderBindingSet = cmdList.CreateShaderBindingSet(
            m_SkyShaderBindingSetLayout.get(),
            {
                {RHIShaderBindingType::TextureSRV, nullptr, m_EnvironmentSRV.get()},
                {RHIShaderBindingType::UniformBuffer, m_SkyFrameUniformBuffer.get(), nullptr},
            });
    }

    void SkyBoxPass::Shutdown()
    {
        m_SkyShader.reset();
        m_CubeVertexBuffer.reset();
        m_CubeVertexLayout.reset();
        m_SkyPipelineState.reset();
        m_SkyPipelineLayout.reset();
        m_SkyShaderBindingSetLayout.reset();
        m_EnvironmentCube.reset();
        m_EnvironmentSRV.reset();
        m_SkyShaderBindingSet.reset();
        m_SkyFrameUniformBuffer.reset();
    }

    void SkyBoxPass::Setup(RenderPassBuilder& builder)
    {
        RDGTextureDesc desc{};
        builder.AddColorOutput(kRDGSceneColor, desc);
        builder.SetDepthStencilOutput(kRDGSceneDepth, desc);
    }

    void SkyBoxPass::PreparePass(RenderGraphFrameResources& frameResources)
    {
        m_ActiveFrameResources = &frameResources;
        m_ShouldRender = false;
        m_DrawPacket = {};

        const FrameRenderGraphContext& context = frameResources.GetFrameContext();
        if (!IsReady() || context.SceneContext == nullptr || context.SceneContext->Camera == nullptr
            || context.SceneContext->Scene == nullptr)
        {
            return;
        }

        if (context.DrawDesc != nullptr
            && !HasSceneDrawFlag(context.DrawDesc->Flags, SceneDrawFlags::EnableSkyBox))
        {
            return;
        }

        SkyBoxSceneProxy* skyBoxProxy = context.SceneContext->Scene->GetSkyBoxProxy();
        if (skyBoxProxy == nullptr || !skyBoxProxy->m_Enabled || skyBoxProxy->m_SkyBoxComponent == nullptr)
        {
            return;
        }

        if (!m_SkyShader || !m_CubeVertexBuffer || !m_SkyPipelineState || !m_SkyShaderBindingSet || !m_SkyFrameUniformBuffer)
        {
            return;
        }

        SkyPassFrameUBO frameData{};
        frameData.Projection = context.SceneContext->Camera->GetProjectionMatrix();
        frameData.View = context.SceneContext->Camera->GetViewMatrix();
        frameData.SkyIntensity = skyBoxProxy->m_SkyIntensity;
        m_SkyFrameUniformBuffer->UpdateSubresource(&frameData, 0, sizeof(SkyPassFrameUBO));

        m_DrawPacket.PipelineState = m_SkyPipelineState;
        m_DrawPacket.ShaderBindingSets[EngineShaderBindings::kSetSkyPass] = m_SkyShaderBindingSet.get();
        m_DrawPacket.VertexBuffer = m_CubeVertexBuffer.get();
        m_ShouldRender = true;
    }

    void SkyBoxPass::BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters)
    {
        (void)parameters;

        if (!m_ShouldRender || !m_ActiveFrameResources || !m_DrawPacket.PipelineState)
        {
            return;
        }

        RHITexture* colorTexture = m_ActiveFrameResources->GetRHI(kRDGSceneColor);
        RHITexture* depthTexture = m_ActiveFrameResources->GetRHI(kRDGSceneDepth);
        if (!colorTexture || !depthTexture)
        {
            return;
        }

        RHIRenderPassInfo passInfo = MakeSceneRenderPassInfo(colorTexture, depthTexture, true);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, colorTexture->GetDesc().Width, colorTexture->GetDesc().Height);
        cmdList.SubmitMeshDrawPacket(m_DrawPacket);
        cmdList.EndRenderPass();

        m_ActiveFrameResources->SetLastKnownUsage(kRDGSceneColor, RDGTextureUsage::RenderTarget);
        m_ActiveFrameResources->SetLastKnownUsage(kRDGSceneDepth, RDGTextureUsage::DepthWrite);
    }

    void SkyBoxPass::Execute(
        RHICommandList& cmdList,
        const RenderCamera& camera,
        const SkyBoxSceneProxy& skyBox)
    {
        if (!m_SkyShader || !m_CubeVertexBuffer || !m_CubeVertexLayout || !m_SkyPipelineState || !m_SkyShaderBindingSetLayout ||
            !m_SkyFrameUniformBuffer || !m_EnvironmentCube || !m_EnvironmentSRV || !m_SkyShaderBindingSet)
        {
            return;
        }

        if (!skyBox.m_Enabled)
        {
            return;
        }

        SkyPassFrameUBO frameData{};
        frameData.Projection = camera.GetProjectionMatrix();
        frameData.View = camera.GetViewMatrix();
        frameData.SkyIntensity = skyBox.m_SkyIntensity;
        m_SkyFrameUniformBuffer->UpdateSubresource(&frameData, 0, sizeof(SkyPassFrameUBO));

        MeshDrawPacket packet;
        packet.PipelineState = m_SkyPipelineState;
        packet.ShaderBindingSets[EngineShaderBindings::kSetSkyPass] = m_SkyShaderBindingSet.get();
        packet.VertexBuffer = m_CubeVertexBuffer.get();
        cmdList.SubmitMeshDrawPacket(packet);
    }
}
