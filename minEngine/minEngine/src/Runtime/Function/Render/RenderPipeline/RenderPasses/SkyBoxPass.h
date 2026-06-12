#pragma once

#include "Core.h"
#include "Render/DrawCommands/MeshDrawPacket.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTexture.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

#include <filesystem>
#include <memory>

namespace minEngine
{
    class RHI;
    class RHICommandList;
    class RHIShaderBindingSetLayout;
    class RHIPipelineLayout;
    class RHIShaderBindingSet;
    class RHIGraphicsPipelineState;
    class RHIShader;
    class RHIShaderResourceView;
    class RenderCamera;
    class SkyBoxSceneProxy;
    class RHIBuffer;
    class RHIVertexInputLayout;
    class TextureCube;
    class RenderGraphFrameResources;
    class RenderPassBuilder;

    class SkyBoxPass : public IRenderPass
    {
    public:
        void Initialize(RHI& rhi, const std::filesystem::path& engineDefaultAssetsRoot);
        void Shutdown();

        void Execute(RHICommandList& cmdList, const RenderCamera& camera, const SkyBoxSceneProxy& skyBox);

        bool IsReady() const
        {
            return m_SkyShader != nullptr && m_CubeVertexBuffer != nullptr && m_EnvironmentCube != nullptr;
        }

        void Setup(RenderPassBuilder& builder) override;
        void PreparePass(RenderGraphFrameResources& frameResources) override;
        void BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters) override;

    private:
        std::shared_ptr<RHIShader> m_SkyShader;
        RHIBufferRef m_CubeVertexBuffer;
        RHIVertexInputLayoutRef m_CubeVertexLayout;
        std::shared_ptr<RHIGraphicsPipelineState> m_SkyPipelineState;
        std::shared_ptr<RHIShaderBindingSetLayout> m_SkyShaderBindingSetLayout;
        std::shared_ptr<RHIPipelineLayout> m_SkyPipelineLayout;
        std::shared_ptr<TextureCube> m_EnvironmentCube;
        std::shared_ptr<RHIShaderResourceView> m_EnvironmentSRV;
        std::shared_ptr<RHIShaderBindingSet> m_SkyShaderBindingSet;
        RHIBufferRef m_SkyFrameUniformBuffer;

        bool m_ShouldRender = false;
        MeshDrawPacket m_DrawPacket;
        RenderGraphFrameResources* m_ActiveFrameResources = nullptr;
    };
}
