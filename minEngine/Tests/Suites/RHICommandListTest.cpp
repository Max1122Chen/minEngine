#include "RHICommandListTest.h"

#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIRenderPass.h"

#include "doctest.h"

#include <memory>

using minEngine::ERHICommandListState;
using minEngine::RHI;
using minEngine::RHICommandList;
using minEngine::RHIRenderPassInfo;

namespace
{
    class CountingRHI final : public RHI
    {
    public:
        int BeginPassCount = 0;
        int EndPassCount = 0;
        int DrawCount = 0;

        void Initialize() override {}
        void Shutdown() override {}

        std::shared_ptr<minEngine::RHITexture> RHICreateTexture2D(
            const minEngine::RHITextureCreateDesc&,
            const void*) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIShaderResourceView> RHICreateShaderResourceView(
            const minEngine::RHITextureSRVDesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIBuffer> RHICreateBuffer(
            const minEngine::RHIBufferCreateDesc&,
            const void*) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIShader> RHICreateShader(
            const minEngine::RHIShaderCreateDesc&,
            std::string*) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIShader> RHICreateShader(
            const std::string&,
            const std::string&,
            std::string*) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIGraphicsPipelineState> RHICreateGraphicsPipelineState(
            const minEngine::RHIGraphicsPSODesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIShaderBindingSetLayout> RHICreateShaderBindingSetLayout(
            const std::vector<minEngine::RHIShaderBindingSetLayoutEntry>&) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIPipelineLayout> RHICreatePipelineLayout(
            const std::vector<minEngine::RHIShaderBindingSetLayout*>&) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIShaderBindingSet> RHICreateShaderBindingSet(
            minEngine::RHIShaderBindingSetLayout*,
            const std::vector<minEngine::RHIShaderBinding>&) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHISampler> RHIGetOrCreateSampler(
            const minEngine::RHISamplerDesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIVertexInputLayout> RHICreateVertexInputLayout(
            std::initializer_list<minEngine::RHIVertexElement>) override
        {
            return nullptr;
        }

        void RHICmdBeginRenderPass(const RHIRenderPassInfo&) override { ++BeginPassCount; }
        void RHICmdEndRenderPass() override { ++EndPassCount; }
        void RHICmdSetGraphicsPipelineState(minEngine::RHIGraphicsPipelineState*) override {}
        void RHICmdSetShaderBindingSet(uint32_t, minEngine::RHIShaderBindingSet*) override {}
        void RHICmdTransition(const minEngine::RHITextureTransitionInfo&) override {}
        void RHICmdSetViewport(uint32_t, uint32_t, uint32_t, uint32_t, bool) override {}
        void RHICmdSetVertexBuffer(minEngine::RHIBuffer*, uint32_t) override {}
        void RHICmdSetIndexBuffer(minEngine::RHIBuffer*) override {}
        void RHICmdDrawIndexed(uint32_t, uint32_t, int32_t) override { ++DrawCount; }
        void RHICmdDraw(uint32_t, uint32_t) override { ++DrawCount; }
        void RHICmdGenerateMips(minEngine::RHITexture*) override {}
        void RHISetBackbufferClearColor(const minEngine::Math::Vector3&) override {}
        void RHIClearBackbuffer() override {}
        void RHIPresent() override {}
    };
}

TEST_CASE("rhi-command-list: immediate is default without Multi [smoke]")
{
    CountingRHI rhi;
    RHICommandList cmdList(&rhi);
    CHECK(cmdList.IsImmediate());
    CHECK(cmdList.GetState() == ERHICommandListState::Idle);

    cmdList.BeginRecording();
    cmdList.EndRenderPass();
    CHECK(rhi.EndPassCount == 1);
    CHECK(cmdList.GetState() == ERHICommandListState::Idle);

    cmdList.FinishRecording();
    cmdList.Replay();
    CHECK(rhi.EndPassCount == 1);
}

TEST_CASE("rhi-command-list: recording defers until Replay [smoke]")
{
    CountingRHI rhi;
    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(false);
    CHECK_FALSE(cmdList.IsImmediate());

    cmdList.BeginRecording();
    CHECK(cmdList.GetState() == ERHICommandListState::Recording);

    RHIRenderPassInfo info{};
    cmdList.BeginRenderPass(info);
    cmdList.Draw(3);
    cmdList.EndRenderPass();
    CHECK(rhi.BeginPassCount == 0);
    CHECK(rhi.DrawCount == 0);
    CHECK(rhi.EndPassCount == 0);

    cmdList.FinishRecording();
    CHECK(cmdList.GetState() == ERHICommandListState::Finished);

    cmdList.Replay();
    CHECK(rhi.BeginPassCount == 1);
    CHECK(rhi.DrawCount == 1);
    CHECK(rhi.EndPassCount == 1);
    CHECK(cmdList.GetState() == ERHICommandListState::Idle);
}
