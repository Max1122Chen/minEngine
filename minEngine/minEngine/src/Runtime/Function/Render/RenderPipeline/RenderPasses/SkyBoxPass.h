#pragma once

#include "Core.h"
#include "Render/DrawCommands/MeshDrawPacket.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTypes.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHITextureViewCache.h"

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
    class RHITexture;
    class RenderGraph;
    class RenderPass;

    class SkyBoxPass : public IRenderPass
    {
    public:
        void Initialize(RHI& rhi, const std::filesystem::path& engineDefaultAssetsRoot);
        void Shutdown();

        void Execute(RHICommandList& cmdList, const RenderCamera& camera, const SkyBoxSceneProxy& skyBox);

        bool IsReady() const
        {
            return m_SkyShader != nullptr && m_CubeVertexBuffer != nullptr && m_SkyPipelineState != nullptr;
        }

        void SetupDependencies(RenderPass& self, RenderGraph& graph) override;
        void Prepare(RenderGraph& graph) override;
        void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override;
        bool NeedRenderPass() const override { return true; }

    private:
        bool BindEnvironmentTexture(RHICommandList& cmdList, RHITexture* environmentTexture);

        std::shared_ptr<RHIShader> m_SkyShader;
        RHIBufferRef m_CubeVertexBuffer;
        RHIVertexInputLayoutRef m_CubeVertexLayout;
        std::shared_ptr<RHIGraphicsPipelineState> m_SkyPipelineState;
        std::shared_ptr<RHIShaderBindingSetLayout> m_SkyShaderBindingSetLayout;
        std::shared_ptr<RHIPipelineLayout> m_SkyPipelineLayout;
        RHITextureViewCache m_TextureViewCache;
        RHITexture* m_BoundEnvironmentTexture = nullptr;
        std::shared_ptr<RHIShaderResourceView> m_EnvironmentSRV;
        std::shared_ptr<RHIShaderBindingSet> m_SkyShaderBindingSet;
        RHIBufferRef m_SkyFrameUniformBuffer;

        bool m_ShouldRender = false;
        MeshDrawPacket m_DrawPacket;
    };
}
