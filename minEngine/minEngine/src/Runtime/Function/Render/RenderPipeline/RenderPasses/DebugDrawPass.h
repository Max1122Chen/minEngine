#pragma once

#include "Core.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTypes.h"
#include "RenderPassBase.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"

#include <memory>

namespace minEngine
{
    class RHI;
    class RHICommandList;
    class RHIShaderBindingSetLayout;
    class RHIPipelineLayout;
    class RHIGraphicsPipelineState;
    class RHIShader;
    class RenderGraph;
    class RenderPass;

    class DebugDrawPass : public RenderPassBase, public IRenderPass
    {
    public:
        DebugDrawPass() = default;
        ~DebugDrawPass() override = default;

        void Initialize();
        void Shutdown();

        void Execute() override;

        void SetupDependencies(RenderPass& self, RenderGraph& graph) override;
        void Prepare(RenderGraph& graph) override;
        void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override;
        bool NeedRenderPass() const override { return m_ShouldRender; }

    private:
        void EnsureVertexBufferCapacity(RHICommandList& cmdList, uint32_t requiredByteSize);

        RHIBufferRef m_VertexBuffer;
        RHIVertexInputLayoutRef m_VertexLayout;
        std::shared_ptr<RHIShader> m_Shader;
        std::shared_ptr<RHIGraphicsPipelineState> m_PipelineState;
        std::shared_ptr<RHIShaderBindingSetLayout> m_BindingSetLayout;
        std::shared_ptr<RHIPipelineLayout> m_PipelineLayout;
        RHIShaderBindingSetRef m_ShaderBindingSet;

        uint32_t m_VertexBufferCapacityBytes = 0;
        uint32_t m_DrawVertexCount = 0;
        bool m_ShouldRender = false;
        bool m_IsReady = false;
    };
}
