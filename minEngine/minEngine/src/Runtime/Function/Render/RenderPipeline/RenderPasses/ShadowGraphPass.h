#pragma once

#include "Core.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

#include <string>

namespace minEngine
{
    class RHITexture;
    class ShadowPass;
    class RenderGraphFrameResources;
    class RenderPassBuilder;
    class RHICommandList;

    /** One graph pass instance for a single ShadowDrawCommand (RND-F01 S04). */
    class ShadowGraphPass : public IRenderPass
    {
    public:
        explicit ShadowGraphPass(ShadowPass& shadowPass);

        void SetSlotNames(std::string passName, std::string depthSlotName);
        void Configure(const ShadowDrawCommand& command);
        void ClearCommand();

        void Setup(RenderPassBuilder& builder) override;
        void PreparePass(RenderGraphFrameResources& frameResources) override;
        void BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters) override;

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
