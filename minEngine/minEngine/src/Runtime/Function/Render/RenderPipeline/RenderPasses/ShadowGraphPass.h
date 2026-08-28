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

    /** RDG depth output declared every frame for a fixed shadow graph slot. */
    struct ShadowGraphPermanentOutput
    {
        std::string DepthResourceName;
        ShadowResourceType ResourceType = ShadowResourceType::Invalid;
        ShadowResolution Resolution{};
        int LayerCount = 1;
        bool IsSet = false;
    };

    /** One graph pass instance for a single ShadowDrawCommand (RND-F01 S04 / F07 stub). */
    class ShadowGraphPass : public IRenderPass
    {
    public:
        explicit ShadowGraphPass(ShadowPass& shadowPass);

        void SetSlotNames(std::string passName, std::string depthSlotName);
        void SetPermanentGraphOutput(ShadowGraphPermanentOutput output);
        void Configure(const ShadowDrawCommand& command);
        void ClearCommand();
        void BindGraphTexture(const RHITextureRef& texture);

        void SetupDependencies(RenderPass& self, RenderGraph& graph) override;
        void Prepare(RenderGraph& graph) override;
        void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override;
        bool NeedRenderPass() const override;

        const std::string& GetDepthSlotName() const { return m_DepthSlotName; }
        const ShadowDrawCommand* GetDrawCommand() const;
        RHITexture* GetShadowTexture() const;

    private:
        void DeclareDepthOutput(RenderPass& self, const ShadowResourceHandle& handle, const std::string& depthName);

        ShadowPass& m_ShadowPass;
        ShadowDrawCommand m_Command{};
        bool m_HasCommand = false;
        ShadowGraphPermanentOutput m_PermanentOutput{};
        std::string m_PassName;
        std::string m_DepthSlotName;
    };
}
