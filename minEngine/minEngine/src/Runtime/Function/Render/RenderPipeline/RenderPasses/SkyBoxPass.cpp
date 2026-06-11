#include "SkyBoxPass.h"

#include "Render/DrawCommands/MeshDrawPacket.h"

#include "../../RenderCamera.h"
#include "../../EngineShaderUtils.h"
#include "../../EnginePassUniforms.h"
#include "../../SkyBoxSceneProxies/SkyBoxSceneProxy.h"
#include "../../TextureCubeLoader.h"
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

        m_SkyPipelineLayout = cmdList.CreatePipelineLayout({m_SkyBindingLayout.get()});

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
        m_SkyBindingSet = cmdList.CreateBindingSet(
            m_SkyBindingLayout.get(),
            {
                {RHIBindingType::TextureSRV, nullptr, m_EnvironmentSRV.get()},
                {RHIBindingType::UniformBuffer, m_SkyFrameUniformBuffer.get(), nullptr},
            });
    }

    void SkyBoxPass::Shutdown()
    {
        m_SkyShader.reset();
        m_CubeVertexBuffer.reset();
        m_CubeVertexLayout.reset();
        m_SkyPipelineState.reset();
        m_SkyPipelineLayout.reset();
        m_SkyBindingLayout.reset();
        m_EnvironmentCube.reset();
        m_EnvironmentSRV.reset();
        m_SkyBindingSet.reset();
        m_SkyFrameUniformBuffer.reset();
    }

    void SkyBoxPass::Execute(
        RHICommandList& cmdList,
        const RenderCamera& camera,
        const SkyBoxSceneProxy& skyBox) const
    {
        if (!m_SkyShader || !m_CubeVertexBuffer || !m_CubeVertexLayout || !m_SkyPipelineState || !m_SkyBindingLayout ||
            !m_SkyFrameUniformBuffer || !m_EnvironmentCube || !m_EnvironmentSRV || !m_SkyBindingSet)
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
        packet.BindingSets[EngineShaderBindings::kSetSkyPass] = m_SkyBindingSet.get();
        packet.VertexBuffer = m_CubeVertexBuffer.get();
        cmdList.SubmitMeshDrawPacket(packet);
    }
}
