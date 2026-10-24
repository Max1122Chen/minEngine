#pragma once

#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIClipSpaceCapabilities.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIPipelineLayout.h"
#include "Render/RHI/RHIResourceTransition.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace minEngine
{
    class MeshDrawCommand;

    enum class ERHICommandListState : uint8_t
    {
        Idle,
        Recording,
        Finished
    };

    /**
     * Concrete forwarding layer (UE FRHICommandList pattern). No per-backend subclass.
     * Single Mode defaults to Immediate (call = RHICmd*). Multi forces Recording + Replay.
     */
    class MINENGINE_API RHICommandList
    {
    public:
        explicit RHICommandList(RHI* rhi);
        RHICommandList() = default;
        ~RHICommandList() = default;

        RHICommandList(const RHICommandList&) = delete;
        RHICommandList& operator=(const RHICommandList&) = delete;
        RHICommandList(RHICommandList&&) = default;
        RHICommandList& operator=(RHICommandList&&) = default;

        RHI* GetExecutingRHI() const { return m_RHI; }

        /** True when cmds execute immediately (Single default). */
        bool IsImmediate() const;
        ERHICommandListState GetState() const { return m_State; }

        /**
         * Test / Multi override. Auto follows FrameThreading Mode
         * (Single → Immediate, Multi → Recording).
         */
        void SetImmediateOverride(bool bImmediate);
        void ClearImmediateOverride();

        void BeginRecording();
        void FinishRecording();
        /** Replay Finished commands on the RHI role. Immediate: no-op. */
        void Replay();
        void Reset();

        // Draw / state commands (Immediate or recorded)
        void BeginRenderPass(const RHIRenderPassInfo& info);
        void EndRenderPass();
        void SetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState);
        void SetShaderBindingSet(uint32_t setIndex, RHIShaderBindingSet* bindingSet);
        void Transition(const RHITextureTransitionInfo& transition);
        void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool flipY = true);
        void SetViewport(
            uint32_t x,
            uint32_t y,
            uint32_t width,
            uint32_t height,
            RHIViewportConvention convention);
        void SetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot = 0);
        void SetIndexBuffer(RHIBuffer* indexBuffer);
        void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0);
        void Draw(uint32_t vertexCount, uint32_t firstVertex = 0);
        void GenerateMips(RHITexture* texture);

        /** Sole draw submit path: PSO → binding sets → VB/IB → Draw. */
        void SubmitMeshDrawPacket(const MeshDrawPacket& packet);

        // Resource creation — always synchronous on the calling thread (S03).
        RHITextureRef CreateTexture2D(const RHITextureCreateDesc& desc, const void* initialData = nullptr);
        RHIShaderResourceViewRef CreateShaderResourceView(const RHITextureSRVDesc& desc);
        RHIBufferRef CreateBuffer(const RHIBufferCreateDesc& desc, const void* initialData = nullptr);
        RHIShaderRef CreateShader(const RHIShaderCreateDesc& desc, std::string* outCompileLog = nullptr);
        RHIShaderRef CreateShader(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outCompileLog = nullptr);
        RHIGraphicsPipelineStateRef CreateGraphicsPipelineState(const RHIGraphicsPSODesc& desc);
        RHIShaderBindingSetLayoutRef CreateShaderBindingSetLayout(
            const std::vector<RHIShaderBindingSetLayoutEntry>& entries);
        RHIPipelineLayoutRef CreatePipelineLayout(const std::vector<RHIShaderBindingSetLayout*>& setLayouts);
        RHIShaderBindingSetRef CreateShaderBindingSet(
            RHIShaderBindingSetLayout* layout,
            const std::vector<RHIShaderBinding>& resources);
        std::shared_ptr<RHIVertexInputLayout> CreateVertexInputLayout(
            std::initializer_list<RHIVertexElement> elements);

    private:
        void RecordOrExecute(std::function<void()> command);
        void AssertCanIssueDrawCommand() const;
        void BindPacketDescriptorSets(const MeshDrawPacket& packet);
        void SubmitDrawBuffers(const MeshDrawPacket& packet);

        RHI* m_RHI = nullptr;
        ERHICommandListState m_State = ERHICommandListState::Idle;
        /** -1 = Auto from FrameThreading; 0 = Recording; 1 = Immediate. */
        int8_t m_ImmediateOverride = -1;
        std::vector<std::function<void()>> m_RecordedCommands;
    };
}
