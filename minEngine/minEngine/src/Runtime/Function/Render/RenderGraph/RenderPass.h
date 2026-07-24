#pragma once

#include "Core.h"
#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RenderPassBuilder.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class RHICommandList;
    class RenderGraph;
    class RenderGraphFrameResources;

    /** RenderGraph step (not Legacy RenderPasses/*, not RHIRenderPass). */
    class RenderPass
    {
    public:
        explicit RenderPass(std::string name);

        const std::string& GetName() const { return m_Name; }
        const std::vector<PassResourceAccess>& GetDeclaredAccesses() const { return m_DeclaredAccesses; }
        bool IsSetupDone() const { return m_SetupDone; }

        void SetImplementation(std::unique_ptr<IRenderPass> implementation);
        void SetImplementation(IRenderPass* implementation);

        void ResetSetup() { m_SetupDone = false; }

        void SetSetup(std::function<void(RenderPassBuilder&)> callback);
        void SetPreparePass(std::function<void(RenderGraphFrameResources&)> callback);
        void SetBuildRenderPass(std::function<void(RHICommandList&, const PassParameters&)> callback);

        void RunSetup(RenderGraph& graph);
        void PreparePass(RenderGraphFrameResources& frameResources) const;
        void BuildRenderPass(RHICommandList& cmdList, const PassParameters& parameters) const;

        void AddDeclaredAccess(PassResourceAccess access);

    private:
        std::string m_Name;
        bool m_SetupDone = false;

        IRenderPass* m_ImplementationPtr = nullptr;
        std::unique_ptr<IRenderPass> m_Implementation;
        std::function<void(RenderPassBuilder&)> m_SetupCallback;
        std::function<void(RenderGraphFrameResources&)> m_PrepareCallback;
        std::function<void(RHICommandList&, const PassParameters&)> m_BuildCallback;

        std::vector<PassResourceAccess> m_DeclaredAccesses;
    };
}
