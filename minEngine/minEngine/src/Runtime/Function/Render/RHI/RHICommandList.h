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

        /**
         * Upload bytes to a buffer as a command.
         *
         * Immediate: exactly the write RHIBuffer::UpdateSubresource performs, right now, with no
         * extra copy. Recording: the payload is copied into the command stream and written when
         * that point of the stream is replayed, so a draw recorded between two uploads reads the
         * first one. The caller's memory is not referenced after the call returns.
         *
         * Recording paths upload through here; the immediate primitive it wraps is not theirs to
         * call, because a write that lands now is ordered before draws that only run later. The
         * payload is copied, the buffer is not: it has to outlive the replay, like the resources of
         * every other recorded command.
         */
        void UpdateBuffer(RHIBuffer* buffer, const void* data, uint32_t offset, uint32_t size);

        /**
         * Writes that landed immediately while a recording was open on the same thread, i.e.
         * uploads that bypassed UpdateBuffer. A steady frame leaves it unchanged; anything else
         * means a record-path call site went back to the immediate primitive.
         */
        static uint64_t GetImmediateUploadWhileRecordingCount();

        /**
         * Commands refused because the list was built and driven on a thread that does not own the
         * graphics role.
         *
         * Running a command inline means driving the backend from the calling thread. When the frame
         * roles are running the list records instead, so the question never comes up; when they are
         * not, the old route was "immediate for any caller", and a worker thread that built a list
         * put the backend's calls on a thread that does not hold the graphics context. Those calls
         * are refused and counted here now, and the refusal is visible in the per-frame work-loss
         * summary alongside the other lost render work.
         */
        static uint64_t GetOffRoleCommandRefusalCount();

        /**
         * True while this thread holds an open recording window, i.e. a recorded stream that has
         * not been replayed yet.
         *
         * A caller that submits work of its own (an immediate command buffer, a bake that renders
         * into a texture and reads it back) cannot do it inside somebody else's window: the two
         * streams would interleave in one command buffer. Such a caller asks this first and
         * refuses instead of writing into a stream it does not own.
         */
        static bool IsRecordingWindowOpenOnThisThread();

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
        /**
         * True when this list would run its commands inline but the caller does not own the graphics
         * role: the state in which a command must be refused rather than handed to the backend.
         * Always false while this list records, and always false while it carries an explicit
         * immediate/recording override, because then the caller decided the route on purpose.
         */
        bool IsOffRoleCaller() const;
        /** Counts one off-role refusal and reports it under the same budget as the other refusals. */
        void RefuseOffRoleCommand() const;
        void RecordOrExecute(std::function<void()> command);
        /**
         * Reports a command issued outside a recording window and tells the caller whether it may
         * be recorded. Kept under its historical name because the engine exports it.
         */
        void AssertCanIssueDrawCommand() const;
        void BindPacketDescriptorSets(const MeshDrawPacket& packet);
        void SubmitDrawBuffers(const MeshDrawPacket& packet);

        RHI* m_RHI = nullptr;
        ERHICommandListState m_State = ERHICommandListState::Idle;
        /** -1 = Auto from FrameThreading; 0 = Recording; 1 = Immediate. */
        int8_t m_ImmediateOverride = -1;
        std::vector<std::function<void()>> m_RecordedCommands;
        /**
         * Copies of recorded upload payloads, one block per recorded upload. The pool hands the
         * same lists back frame after frame, so the blocks are reused instead of reallocated. A
         * block a recorded command points into is not touched again until that recording is reset.
         */
        std::vector<std::vector<uint8_t>> m_UploadScratch;
        size_t m_UploadScratchCursor = 0;
        /** True while this list keeps the current thread's recording window open. */
        bool m_HoldsRecordingWindow = false;
    };

    /**
     * Engine-internal entry the backend primitive behind RHIBuffer::UpdateSubresource reports to,
     * so a record-path upload that lands immediately becomes visible instead of silently running
     * out of order. Deliberately not part of the export contract: nothing outside the engine DLL
     * calls it.
     */
    void ReportImmediateBufferUpload(const char* target);
}
