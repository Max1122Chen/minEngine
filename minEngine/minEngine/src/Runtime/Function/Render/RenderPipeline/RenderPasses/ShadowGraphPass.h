#pragma once

#include "Core.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

#include <string>

namespace minEngine
{
    class RHITexture;
    class ShadowPass;
    class RHICommandList;
    class RenderGraph;
    class RenderPass;

    /** One graph pass instance for a single ShadowDrawCommand (RND-F01 S04 / F07 stub). */
    class ShadowGraphPass : public IRenderPass
    {
    public:
        explicit ShadowGraphPass(ShadowPass& shadowPass);

        void SetSlotNames(std::string passName, std::string depthSlotName);
        void Configure(const ShadowDrawCommand& command);
        void ClearCommand();

        void SetupDependencies(RenderPass& self, RenderGraph& graph) override;
        void Prepare(RenderGraph& graph) override;
        void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override;
        bool NeedRenderPass() const override;

        const std::string& GetDepthSlotName() const { return m_DepthSlotName; }
        RHITexture* GetShadowTexture() const;

    private:
        ShadowPass& m_ShadowPass;
        ShadowDrawCommand m_Command{};
        bool m_HasCommand = false;
        std::string m_PassName;
        std::string m_DepthSlotName;
    };
}
