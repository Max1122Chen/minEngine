#include "SkyBoxPass.h"

#include "Render/DrawCommands/MeshDrawPacket.h"
#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Render/RenderGraph/SceneRenderPassUtils.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"
#include "Runtime/Function/Render/SceneRenderContext.h"

#include "../../RenderCamera.h"
#include "../../EngineShaderUtils.h"
#include "../../EnginePassUniforms.h"
#include "../../SkyBoxSceneProxies/SkyBoxSceneProxy.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/Environment/EnvironmentMap.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/Texture.h"

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
    }

    void SkyBoxPass::Shutdown()
    {
        m_SkyShader.reset();
        m_CubeVertexBuffer.reset();
        m_CubeVertexLayout.reset();
        m_SkyPipelineState.reset();
        m_SkyPipelineLayout.reset();
        m_SkyShaderBindingSetLayout.reset();
        m_BoundEnvironmentTexture = nullptr;
        m_EnvironmentSRV.reset();
        m_SkyShaderBindingSet.reset();
        m_SkyFrameUniformBuffer.reset();
        m_TextureViewCache.Clear();
    }

    bool SkyBoxPass::BindEnvironmentTexture(RHICommandList& cmdList, RHITexture* environmentTexture)
    {
        if (!m_SkyShaderBindingSetLayout || !m_SkyFrameUniformBuffer || environmentTexture == nullptr)
        {
            return false;
        }

        if (environmentTexture == m_BoundEnvironmentTexture && m_SkyShaderBindingSet)
        {
            return true;
        }

        m_BoundEnvironmentTexture = environmentTexture;
        m_EnvironmentSRV = m_TextureViewCache.GetOrCreate(cmdList, environmentTexture);
        m_SkyShaderBindingSet = cmdList.CreateShaderBindingSet(
            m_SkyShaderBindingSetLayout.get(),
            {
                {RHIShaderBindingType::TextureSRV, nullptr, m_EnvironmentSRV.get()},
                {RHIShaderBindingType::UniformBuffer, m_SkyFrameUniformBuffer.get(), nullptr},
            });
        return m_SkyShaderBindingSet != nullptr;
    }

    void SkyBoxPass::SetupDependencies(RenderPass& self, RenderGraph& graph)
    {
        (void)graph;
        RDGAttachmentInfo color{};
        color.SizeClass = RDGSizeClass::SwapchainRelative;
        color.SizeX = 1.0f;
        color.SizeY = 1.0f;
        color.Format = TextureFormat::RGBA8;
        RDGAttachmentInfo depth{};
        depth.SizeClass = RDGSizeClass::SwapchainRelative;
        depth.SizeX = 1.0f;
        depth.SizeY = 1.0f;
        depth.Format = TextureFormat::DEPTH24STENCIL8;
        self.AddColorOutput(kRDGSceneColor, color);
        self.SetDepthStencilOutput(kRDGSceneDepth, depth);
    }

    void SkyBoxPass::Prepare(RenderGraph& graph)
    {
        m_ShouldRender = false;
        m_DrawPacket = {};

        const RenderGraphFrameContext& context = graph.GetFrameContext();
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

        if (!m_SkyShader || !m_CubeVertexBuffer || !m_SkyPipelineState || !m_SkyFrameUniformBuffer)
        {
            return;
        }

        EnvironmentMap* environmentMap = skyBoxProxy->m_EnvironmentMap.get();
        if (environmentMap == nullptr || context.CommandList == nullptr)
        {
            return;
        }

        RHI* rhi = context.CommandList->GetExecutingRHI();
        if (rhi == nullptr)
        {
            return;
        }

        if (!environmentMap->EnsureGPUResources(*rhi) || !environmentMap->IsReadyForSky())
        {
            return;
        }

        if (!BindEnvironmentTexture(*context.CommandList, environmentMap->GetEnvironment()->GetRHITexture()))
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

    void SkyBoxPass::BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph)
    {
        RHITexture* colorTexture = graph.TryGetPhysicalTexture(&graph.GetTextureResource(kRDGSceneColor));
        RHITexture* depthTexture = graph.TryGetPhysicalTexture(&graph.GetTextureResource(kRDGSceneDepth));
        if (colorTexture == nullptr || depthTexture == nullptr)
        {
            return;
        }

        RHIRenderPassInfo passInfo = MakeSceneRenderPassInfo(colorTexture, depthTexture, true);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, colorTexture->GetDesc().Width, colorTexture->GetDesc().Height);
        if (m_ShouldRender && m_DrawPacket.PipelineState)
        {
            cmdList.SubmitMeshDrawPacket(m_DrawPacket);
        }
        cmdList.EndRenderPass();
    }

    void SkyBoxPass::Execute(
        RHICommandList& cmdList,
        const RenderCamera& camera,
        const SkyBoxSceneProxy& skyBox)
    {
        if (!m_SkyShader || !m_CubeVertexBuffer || !m_CubeVertexLayout || !m_SkyPipelineState
            || !m_SkyFrameUniformBuffer || !skyBox.m_Enabled || !skyBox.m_EnvironmentMap)
        {
            return;
        }

        RHI* rhi = cmdList.GetExecutingRHI();
        if (rhi == nullptr
            || !skyBox.m_EnvironmentMap->EnsureGPUResources(*rhi)
            || !skyBox.m_EnvironmentMap->IsReadyForSky())
        {
            return;
        }

        if (!BindEnvironmentTexture(cmdList, skyBox.m_EnvironmentMap->GetEnvironment()->GetRHITexture()))
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
