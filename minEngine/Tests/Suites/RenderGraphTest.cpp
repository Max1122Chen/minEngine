#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderGraphFrameResources.h"
#include "Render/RenderGraph/RenderPassBuilder.h"
#include "Render/RHI/RHICommandList.h"
#include "doctest.h"

namespace minEngine
{
    TEST_CASE("render-graph: stub pass setup and execution order [smoke]")
    {
        RenderGraph graph;
        RenderPass& stubPass = graph.AddPass("Stub");

        int prepareCount = 0;
        int buildCount = 0;

        stubPass.SetSetup([](RenderPassBuilder& builder) {
            RDGTextureDesc desc{};
            desc.Width = 64;
            desc.Height = 64;
            builder.AddColorOutput("StubColor", desc);
        });
        stubPass.SetPreparePass([&prepareCount](RenderGraphFrameResources&) { ++prepareCount; });
        stubPass.SetBuildRenderPass([&buildCount](RHICommandList&, const PassParameters&) { ++buildCount; });

        RenderGraphFrameResources frameResources;
        graph.SetupAttachments(frameResources);

        CHECK(stubPass.IsSetupDone());
        CHECK(stubPass.GetDeclaredAccesses().size() == 1);
        CHECK(stubPass.GetDeclaredAccesses()[0].TextureName == "StubColor");
        CHECK(
            stubPass.GetDeclaredAccesses()[0].AccessType == RDGPassResourceAccessType::ColorOutput);

        const RenderPass* executionOrder[] = {&stubPass};
        graph.SetPassExecutionOrder(executionOrder, 1);

        RHICommandList cmdList(nullptr);
        graph.ExecuteGraph(cmdList, frameResources);

        CHECK(prepareCount == 1);
        CHECK(buildCount == 1);
    }

    TEST_CASE("render-graph: prepare runs before build across passes [smoke]")
    {
        RenderGraph graph;
        RenderPass& firstPass = graph.AddPass("First");
        RenderPass& secondPass = graph.AddPass("Second");

        std::string trace;
        firstPass.SetPreparePass([&trace](RenderGraphFrameResources&) { trace += "P0;"; });
        firstPass.SetBuildRenderPass([&trace](RHICommandList&, const PassParameters&) { trace += "B0;"; });
        secondPass.SetPreparePass([&trace](RenderGraphFrameResources&) { trace += "P1;"; });
        secondPass.SetBuildRenderPass([&trace](RHICommandList&, const PassParameters&) { trace += "B1;"; });

        RenderGraphFrameResources setupResources;
        graph.SetupAttachments(setupResources);

        const RenderPass* executionOrder[] = {&firstPass, &secondPass};
        graph.SetPassExecutionOrder(executionOrder, 2);

        RenderGraphFrameResources frameResources;
        RHICommandList cmdList(nullptr);
        graph.ExecuteGraph(cmdList, frameResources);

        CHECK(trace == "P0;P1;B0;B1;");
    }
}
