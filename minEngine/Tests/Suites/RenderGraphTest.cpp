#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderGraphFrameResources.h"
#include "Render/RenderGraph/RenderPassBuilder.h"
#include "Render/RHI/RHICommandList.h"
#include "doctest.h"

#include <stdexcept>

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

        RenderGraphFrameResources frameResources;
        RHICommandList cmdList(nullptr);
        graph.ExecuteGraph(cmdList, frameResources);

        CHECK(trace == "P0;P1;B0;B1;");
    }

    TEST_CASE("render-graph: bake derives dependency order from declared resources [smoke]")
    {
        RenderGraph graph;
        RenderPass& producerPass = graph.AddPass("Producer");
        RenderPass& consumerPass = graph.AddPass("Consumer");

        std::string trace;
        producerPass.SetSetup([](RenderPassBuilder& builder) {
            RDGTextureDesc desc{};
            desc.Width = 32;
            desc.Height = 32;
            builder.AddColorOutput("SceneColor", desc);
        });
        producerPass.SetPreparePass([&trace](RenderGraphFrameResources&) { trace += "PP;"; });
        producerPass.SetBuildRenderPass([&trace](RHICommandList&, const PassParameters&) { trace += "BP;"; });

        consumerPass.SetSetup([](RenderPassBuilder& builder) {
            builder.AddTextureInput("SceneColor");
        });
        consumerPass.SetPreparePass([&trace](RenderGraphFrameResources&) { trace += "PC;"; });
        consumerPass.SetBuildRenderPass([&trace](RHICommandList&, const PassParameters&) { trace += "BC;"; });

        const RenderPass* selectedPasses[] = {&consumerPass, &producerPass};
        graph.SetPassExecutionOrder(selectedPasses, 2);

        RenderGraphFrameResources frameResources;
        graph.SetupAttachments(frameResources);

        RHICommandList cmdList(nullptr);
        graph.ExecuteGraph(cmdList, frameResources);

        CHECK(trace == "PP;PC;BP;BC;");
    }

    TEST_CASE("render-graph: bake rejects missing producer [smoke]")
    {
        RenderGraph graph;
        RenderPass& consumerPass = graph.AddPass("Consumer");
        consumerPass.SetSetup([](RenderPassBuilder& builder) {
            builder.AddTextureInput("MissingColor");
        });

        const RenderPass* selectedPasses[] = {&consumerPass};
        graph.SetPassExecutionOrder(selectedPasses, 1);

        RenderGraphFrameResources frameResources;
        graph.SetupAttachments(frameResources);

        CHECK_THROWS_AS(graph.Bake(), std::logic_error);
    }
}
