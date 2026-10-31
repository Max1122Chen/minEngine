#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/RenderPassBase.h"
#include "Math/Math.h"
#include "Runtime/Function/Render/EnginePassUniforms.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowUniformBuffers.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/SceneRenderContext.h"

#include <unordered_map>

namespace minEngine
{
    class RHI;
    class MeshDrawCommand;
    class RHIBuffer;
    class RHIShader;
    class RHICommandList;
    class RHIGraphicsPipelineState;
    class RHIShaderBindingSetLayout;
    class RHIShaderBindingSet;
    class RHIVertexInputLayout;

    class ShadowPass : public RenderPassBase
    {
    public:
        ShadowPass() = default;
        virtual ~ShadowPass() = default;

        void Initialize();

        virtual void Execute() override;
        void Execute(RHICommandList& cmdList);

        void PrepareShadowPass(RHICommandList& cmdList);
        void RenderSingleDrawCommand(RHICommandList& cmdList, const ShadowDrawCommand& command);

    private:
        void Render(RHICommandList& cmdList);
        void Render();
        void DrawOpaqueMeshes(RHICommandList& cmdList, const ShadowPassUniformBinding& uniformBinding);
        void DrawShadowCaster(
            RHICommandList& cmdList,
            const ShadowPassUniformBinding& uniformBinding,
            const MeshDrawCommand& drawCommand,
            std::vector<RHIShaderBinding>& resources);
        RHIGraphicsPipelineStateRef GetOrCreateShadowPipelineForLayout(
            RHIVertexInputLayout* vertexInputLayout,
            RHICommandList& cmdList);

    public:
        RHIBuffer* m_PerObjectUniformBuffer = nullptr;
        const ShadowUniformBuffers* m_ShadowUniformBuffers = nullptr;
        /**
         * The frame's caster queue, borrowed for as long as the frame records.
         *
         * The queue belongs to the frame context, which the pipeline keeps alive until the frame's
         * recording window has been submitted and replayed; this pass only names it. The borrow is
         * what replaces the copy this member used to hold, and the two runs it carries are the
         * shadow draws' order: main-view casters first, shadow-only casters after them.
         */
        ShadowCasterBorrow m_CasterQueue;

        std::vector<ShadowDrawCommand> m_ShadowDrawCommands;

    private:
        std::shared_ptr<RHIShader> m_DepthShader;
        RHIGraphicsPSODesc m_ShadowPSODescTemplate{};
        std::unordered_map<RHIVertexInputLayout*, std::shared_ptr<RHIGraphicsPipelineState>> m_ShadowPipelineByLayout;
        /** Keep per-draw shadow sets alive until the CB submits (Vulkan descriptor lifetime). */
        std::vector<RHIShaderBindingSetRef> m_PendingShadowBindingSets;
        void RenderDirectionalShadow(RHICommandList& cmdList, const ShadowDrawCommand& command);
        void RenderSpotShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand);
        void RenderPointShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand);
    };
}
