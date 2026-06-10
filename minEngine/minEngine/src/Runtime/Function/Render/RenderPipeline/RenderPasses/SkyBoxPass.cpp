#include "SkyBoxPass.h"

#include "../../Environment/EngineIBLEnvironment.h"
#include "../../RenderCamera.h"
#include "../../EngineShaderUtils.h"
#include "../../EnginePassUniforms.h"
#include "../../SkyBoxSceneProxies/SkyBoxSceneProxy.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBinding.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/Texture.h"

#include "Runtime/Function/Render/OpenGL/OpenGLRHIResources.h"

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

        m_SkyBindingLayout = cmdList.CreateBindingLayout({
            {EngineShaderBindings::kSkyPass_EnvironmentSRV,
             RHIBindingType::TextureSRV,
             EngineShaderBindings::kGL_SkyEnvironmentUnit,
             RHIGraphicsShaderStage::Pixel},
            {EngineShaderBindings::kSkyPass_FrameData,
             RHIBindingType::UniformBuffer,
             EngineShaderBindings::kGL_SkyFrameDataUBO,
             RHIGraphicsShaderStage::Vertex},
        });

        RHIBufferCreateDesc frameDesc;
        frameDesc.Usage = RHIBufferUsage::Uniform;
        frameDesc.ByteSize = sizeof(SkyPassFrameUBO);
        m_SkyFrameUniformBuffer = cmdList.CreateBuffer(frameDesc, nullptr);

        RHIGraphicsPSODesc psoDesc;
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
        m_SkyBindingLayout.reset();
        m_EnvironmentSRV.reset();
        m_SkyBindingSet.reset();
        m_SkyFrameUniformBuffer.reset();
    }

    void SkyBoxPass::Execute(
        RHICommandList& cmdList,
        const RenderCamera& camera,
        const SkyBoxSceneProxy& skyBox,
        const EngineIBLEnvironment& iblEnvironment) const
    {
        if (!m_SkyShader || !m_CubeVertexBuffer || !m_CubeVertexLayout || !m_SkyPipelineState || !m_SkyBindingLayout ||
            !m_SkyFrameUniformBuffer)
        {
            return;
        }

        if (!skyBox.m_Enabled)
        {
            return;
        }

        const TextureCube* environment = iblEnvironment.GetEnvironment();
        RHITexture* environmentTexture = environment ? environment->GetRHITexture() : nullptr;
        if (!environmentTexture)
        {
            return;
        }

        RHITextureSRVDesc srvDesc;
        srvDesc.Texture = environmentTexture;
        m_EnvironmentSRV = std::make_shared<OpenGLRHIShaderResourceView>(srvDesc);
        SkyPassFrameUBO frameData{};
        frameData.Projection = camera.GetProjectionMatrix();
        frameData.View = camera.GetViewMatrix();
        frameData.SkyIntensity = skyBox.m_SkyIntensity;
        m_SkyFrameUniformBuffer->UpdateSubresource(&frameData, 0, sizeof(SkyPassFrameUBO));

        m_SkyBindingSet = cmdList.CreateBindingSet(
            m_SkyBindingLayout.get(),
            {
                {RHIBindingType::TextureSRV, nullptr, m_EnvironmentSRV.get()},
                {RHIBindingType::UniformBuffer, m_SkyFrameUniformBuffer.get(), nullptr},
            });

        cmdList.SetGraphicsPipelineState(m_SkyPipelineState.get());
        cmdList.SetBindingSet(EngineShaderBindings::kSetSkyPass, m_SkyBindingSet.get());

        cmdList.SetVertexInputLayout(m_CubeVertexLayout.get());
        cmdList.SetVertexBuffer(m_CubeVertexBuffer.get());
        cmdList.Draw(m_CubeVertexBuffer->GetDesc().ElementCount, 0);
    }
}
