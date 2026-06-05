#include "SkyBoxPass.h"

#include "../../Environment/EngineIBLEnvironment.h"
#include "../../RenderCamera.h"
#include "../../Shader.h"
#include "../../SkyBoxSceneProxies/SkyBoxSceneProxy.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/Texture.h"

#include "Runtime/Function/Render/OpenGL/OpenGLRHIModern.h"
#include "Runtime/Function/Render/OpenGL/OpenGLShader.h"

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

        constexpr int kSkyboxTextureUnit = 0;
    }

    void SkyBoxPass::Initialize(RHI& rhi, const std::filesystem::path& engineDefaultAssetsRoot)
    {
        Shutdown();

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        std::shared_ptr<Shader> skyShader = Shader::CreateFromFiles(
            rhi,
            shaderDirectory / "background.vert",
            shaderDirectory / "background.frag");
        if (!skyShader || !skyShader->IsValid())
        {
            ME_CORE_ERROR("SkyBoxPass: failed to compile background shader.");
            return;
        }

        m_SkyShader = skyShader->GetRHIShader();
        if (auto glLegacy = std::dynamic_pointer_cast<OpenGLShader>(m_SkyShader))
        {
            m_SkyShaderRHI = std::make_shared<OpenGLRHIShader>(glLegacy);
        }

        const uint32_t vertexByteSize = static_cast<uint32_t>(sizeof(kCubeVertices));
        const uint32_t vertexCount = vertexByteSize / (3 * sizeof(float));
        m_CubeVertexBuffer =
            rhi.CreateVertexBuffer(const_cast<float*>(kCubeVertices), vertexByteSize, vertexCount);
        m_CubeVertexDefinition = rhi.CreateVertexDefinition({
            {"a_Position", VertexElementType::Float3, false},
        });
        m_CubeVertexLayout = OpenGLRHIVertexInputLayout::WrapLegacyVertexDefinition(m_CubeVertexDefinition);

        RHICommandList cmdList(&rhi);
        RHIGraphicsPSODesc psoDesc;
        psoDesc.VertexShader = m_SkyShaderRHI.get();
        psoDesc.PixelShader = m_SkyShaderRHI.get();
        psoDesc.VertexInputLayout = m_CubeVertexLayout.get();
        psoDesc.DepthStencilState.bDepthTestEnabled = true;
        psoDesc.DepthStencilState.bDepthWriteEnabled = false;
        psoDesc.DepthStencilState.DepthCompare = RHIDepthCompareFunc::LessEqual;
        m_SkyPipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);
    }

    void SkyBoxPass::Shutdown()
    {
        m_SkyShader.reset();
        m_SkyShaderRHI.reset();
        m_CubeVertexBuffer.reset();
        m_CubeVertexDefinition.reset();
        m_CubeVertexLayout.reset();
        m_SkyPipelineState.reset();
    }

    void SkyBoxPass::Execute(
        RHICommandList& cmdList,
        const RenderCamera& camera,
        const SkyBoxSceneProxy& skyBox,
        const EngineIBLEnvironment& iblEnvironment) const
    {
        if (!m_SkyShader || !m_CubeVertexBuffer || !m_CubeVertexLayout || !m_SkyPipelineState)
        {
            return;
        }

        if (!skyBox.m_Enabled)
        {
            return;
        }

        const TextureCube* environment = iblEnvironment.GetEnvironment();
        if (!environment || !environment->GetRHITexture())
        {
            return;
        }

        cmdList.SetGraphicsPipelineState(m_SkyPipelineState.get());

        m_SkyShader->Use();
        m_SkyShader->UploadUniformMat4("u_Projection", camera.GetProjectionMatrix());
        m_SkyShader->UploadUniformMat4("u_View", camera.GetViewMatrix());
        m_SkyShader->UploadUniformFloat("u_SkyIntensity", skyBox.m_SkyIntensity);

        environment->GetRHITexture()->Bind(kSkyboxTextureUnit);
        m_SkyShader->UploadUniformInt("u_Skybox", kSkyboxTextureUnit);

        cmdList.SetVertexInputLayout(m_CubeVertexLayout.get());
        if (auto vertexBuffer = OpenGLRHIBuffer::WrapLegacyVertexBuffer(m_CubeVertexBuffer))
        {
            cmdList.SetVertexBuffer(vertexBuffer.get());
        }
        cmdList.Draw(m_CubeVertexBuffer->GetNumVertices(), 0);

        environment->GetRHITexture()->Unbind();
    }
}
