#include "RHICommandList.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Core/Threading/ThreadIdentity.h"
#include "Runtime/Function/Render/RHI/RHIDeferredCreate.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

namespace minEngine
{
    namespace
    {
        /**
         * Recordings open on this thread. A write that lands while one is open is ordered before
         * the draws that recording holds, so those draws read the newer bytes at Replay.
         */
        thread_local uint32_t t_OpenRecordingWindowCount = 0;

        std::atomic<uint64_t> g_ImmediateUploadWhileRecordingCount{0};

        std::atomic<uint64_t> g_CommandOutsideRecordingCount{0};

        std::atomic<uint64_t> g_OffRoleCommandRefusalCount{0};

        /** Sets that recorded streams of the live lists keep alive (see GetRecordedBindingSetHoldCount). */
        std::atomic<uint64_t> g_RecordedBindingSetHoldCount{0};

        /** Draws refused because the geometry they named had no native object yet. */
        std::atomic<uint64_t> g_DrawWithoutNativeGeometryCount{0};

        /**
         * Whether a draw may name this handle, reported once per hundred refusals.
         *
         * A buffer created off the graphics role exists as a handle at once and as a native object
         * only after that role's frame start. Submitting the draw before that point names nothing:
         * the backend then either skips the draw or, worse, hands the driver a name that identifies
         * no data. The frame is skipped instead, and the count is what makes a steady stream of
         * skipped frames visible instead of a silent hole in the picture.
         */
        bool CanNameGeometry(const RHIDeferredCreateTarget* target, const char* kind, uint32_t count)
        {
            const RHIDeferredUse use = GetDeferredUse(target);
            if (use == RHIDeferredUse::Usable)
            {
                return true;
            }

            const uint64_t refused =
                g_DrawWithoutNativeGeometryCount.fetch_add(1, std::memory_order_relaxed) + 1;
            if (refused != 1 && refused % 100 != 0)
            {
                return false;
            }

            if (use == RHIDeferredUse::Unusable)
            {
                ME_LOG(
                    LogRender,
                    Error,
                    "Draw not submitted: the bound {} ({:p}) has no native object and none is coming, "
                    "so the draw would name no data ({} requested). {} draw(s) refused so far.",
                    kind,
                    static_cast<const void*>(target),
                    count,
                    refused);
                return false;
            }

            ME_LOG(
                LogRender,
                Warn,
                "Draw not submitted: the bound {} ({:p}) is still waiting for the graphics role to "
                "create its native object, so this frame would name no data ({} requested). The "
                "creation runs at the frame start of the graphics role; the draw resumes on the "
                "frame after it. {} draw(s) refused so far.",
                kind,
                static_cast<const void*>(target),
                count,
                refused);
            return false;
        }

        /**
         * The topology that makes every list record: Multi with the frame roles running. Every other
         * topology - Single, headless, and Multi whose roles have stopped - lets a list run its
         * commands inline, which is why the caller's thread matters in exactly those states.
         */
        bool RecordingTopologyActive()
        {
            return FrameThreading::HasInstance() && FrameThreading::Get().IsMulti()
                && TaskSystem::HasInstance() && TaskSystem::Get().AreFrameRolesRunning();
        }

        const char* CommandListStateName(ERHICommandListState state)
        {
            switch (state)
            {
            case ERHICommandListState::Idle:
                return "Idle";
            case ERHICommandListState::Recording:
                return "Recording";
            case ERHICommandListState::Finished:
                return "Finished";
            }
            return "unknown";
        }

        /**
         * Names the caller and the thread it ran on, plus the frames that lead to it.
         *
         * A single sentence says what was wrong but not who did it, and that is exactly the part a
         * reader cannot recover from the message: the same command is correct in a recording window
         * on one thread and a defect outside one, and the offending call site is one of hundreds.
         * The addresses are printed un-symbolised, against the module that holds them, so the log
         * line stays readable after a rebuild.
         */
        std::string DescribeRecordingCaller()
        {
            const ThreadIdentity identity = CaptureCurrentThreadIdentity();
            char header[256]{};
            std::snprintf(
                header,
                sizeof(header),
                " thread=%u role=%s",
                static_cast<unsigned>(identity.LogThreadId),
                identity.RoleName);

            std::string text(header);
            void* frames[kDiagnosticFrameCount]{};
            const std::size_t frameCount = CaptureDiagnosticFrames(frames, kDiagnosticFrameCount);
            // The first frames are this helper, the reporter and the command entry point.
            for (std::size_t frameIndex = 3; frameIndex < frameCount; ++frameIndex)
            {
                char location[320]{};
                FormatAddressForLog(location, sizeof(location), frames[frameIndex]);
                text += "\n    #" + std::to_string(frameIndex - 3) + " " + location;
            }
            return text;
        }

        void EnterRecordingWindow(bool& held)
        {
            if (held)
            {
                return;
            }
            held = true;
            ++t_OpenRecordingWindowCount;
        }

        /** Closes a recording window a list holds, so Reset and Replay cannot count it twice. */
        void LeaveRecordingWindow(bool& held)
        {
            if (!held)
            {
                return;
            }
            held = false;
            ME_ASSERT(t_OpenRecordingWindowCount > 0, "recording window released without being entered");
            --t_OpenRecordingWindowCount;
        }

        /**
         * Storage for one recorded upload payload. The cursor reaches a block once per recording,
         * so a block is never resized while a recorded command points into it, and the pool
         * handing the same list back next frame reuses the block instead of allocating again.
         */
        uint8_t* AcquireUploadScratchBlock(
            std::vector<std::vector<uint8_t>>& scratch, size_t& cursor, uint32_t size)
        {
            if (cursor == scratch.size())
            {
                scratch.emplace_back();
            }

            std::vector<uint8_t>& block = scratch[cursor++];
            if (block.size() < size)
            {
                block.resize(size);
            }
            return block.data();
        }
    }

    void ReportImmediateBufferUpload(const char* target)
    {
        if (t_OpenRecordingWindowCount == 0)
        {
            return;
        }

        const uint64_t count = g_ImmediateUploadWhileRecordingCount.fetch_add(1) + 1;
        ME_LOG(
            LogRender,
            Error,
            "Buffer upload ran immediately while a command list was recording ({}); it lands before "
            "the draws that recording holds, which then read the newer bytes. Record-path uploads "
            "belong on RHICommandList::UpdateBuffer. Bypasses so far: {}.",
            target != nullptr ? target : "unknown buffer",
            count);
    }

    RHICommandList::RHICommandList(RHI* rhi)
        : m_RHI(rhi)
    {
    }

    bool RHICommandList::IsImmediate() const
    {
        if (m_ImmediateOverride >= 0)
        {
            return m_ImmediateOverride != 0;
        }
        // Multi Recording is active only after TaskSystem frame roles are running.
        if (RecordingTopologyActive())
        {
            return false;
        }
        // Nothing records, so this list would run its commands inline - and running inline means
        // driving the graphics API from this thread, which only the graphics role may do. "The roles
        // stopped" is not the same question as "this thread may talk to the device": once they stop,
        // the old answer was yes for every thread, so a worker that built a list put the backend's
        // calls on a thread that does not hold the context.
        return RHIGraphicsRole::IsCurrent();
    }

    bool RHICommandList::IsOffRoleCaller() const
    {
        if (m_ImmediateOverride >= 0)
        {
            return false;
        }
        return !RecordingTopologyActive() && !RHIGraphicsRole::IsCurrent();
    }

    void RHICommandList::RefuseOffRoleCommand() const
    {
        // Reported, not asserted: the frame this call belongs to can still be drawn once the caller
        // moves to the graphics role, and taking the process down over it would lose the session that
        // has the reproduction in it.
        const uint64_t refused = g_OffRoleCommandRefusalCount.fetch_add(1, std::memory_order_relaxed) + 1;
        // The first one, then one in a hundred: a wrong-thread submission issues many commands, and a
        // line per command would bury the first.
        if (refused == 1 || refused % 100 == 0)
        {
            ME_LOG(
                LogRender,
                Error,
                "RHICommandList command refused: this list was driven from a thread that does not own "
                "the graphics role while no frame roles are running, so running it inline would send "
                "the backend's calls to a thread that does not hold the graphics context. Build, "
                "record and replay the list on the graphics role. {} command(s) refused so far; "
                "caller:{}",
                refused,
                DescribeRecordingCaller());
        }
    }

    uint64_t RHICommandList::GetOffRoleCommandRefusalCount()
    {
        return g_OffRoleCommandRefusalCount.load(std::memory_order_relaxed);
    }

    void RHICommandList::SetImmediateOverride(bool bImmediate)
    {
        m_ImmediateOverride = bImmediate ? 1 : 0;
    }

    void RHICommandList::ClearImmediateOverride()
    {
        m_ImmediateOverride = -1;
    }

    void RHICommandList::BeginRecording()
    {
        if (IsImmediate())
        {
            return;
        }
        if (IsOffRoleCaller())
        {
            // Recording is not a way around the role contract here: the stream this would open ends
            // up replayed by whoever holds the list, and the caller that opened it never could.
            RefuseOffRoleCommand();
            return;
        }
        ME_ASSERT(m_State == ERHICommandListState::Idle, "BeginRecording requires Idle");
        m_RecordedCommands.clear();
        m_UploadScratchCursor = 0;
        m_State = ERHICommandListState::Recording;
        EnterRecordingWindow(m_HoldsRecordingWindow);
    }

    void RHICommandList::FinishRecording()
    {
        if (IsImmediate())
        {
            return;
        }
        ME_ASSERT(m_State == ERHICommandListState::Recording, "FinishRecording requires Recording");
        m_State = ERHICommandListState::Finished;
        // The window closes with the stream it describes, on the thread that opened it. Leaving it
        // to Replay would tie the window to whoever replays, which in a frame-threaded topology is
        // another thread: the recording thread would keep the window open and report every later
        // upload it makes as a write that bypassed a recording.
        LeaveRecordingWindow(m_HoldsRecordingWindow);
    }

    void RHICommandList::Replay()
    {
        if (IsImmediate())
        {
            return;
        }
        if (IsOffRoleCaller())
        {
            // Replaying drives the backend exactly as an inline command does, so a stream handed to a
            // thread that does not own the graphics role is refused, not run here.
            RefuseOffRoleCommand();
            return;
        }
        ME_ASSERT(m_State == ERHICommandListState::Finished, "Replay requires Finished");
        ME_ASSERT(m_RHI != nullptr, "Replay requires an executing RHI");
        // FinishRecording already closed the window; this is the same give-back for a stream that
        // reaches Replay by another route. Either way no window is open here, so the uploads the
        // commands below perform are this stream's own and arrive where they belong in it.
        LeaveRecordingWindow(m_HoldsRecordingWindow);
        for (std::function<void()>& command : m_RecordedCommands)
        {
            if (command)
            {
                command();
            }
        }
        Reset();
    }

    void RHICommandList::Reset()
    {
        LeaveRecordingWindow(m_HoldsRecordingWindow);
        // The commands go with their captures, so the sets they held go with them: the reference a
        // recorded bind took is given back exactly once, whether the command ran or never will.
        if (m_RecordedBindingSetHolds != 0)
        {
            g_RecordedBindingSetHoldCount.fetch_sub(m_RecordedBindingSetHolds, std::memory_order_relaxed);
            m_RecordedBindingSetHolds = 0;
        }
        // The geometry goes with the commands that named it, live or never replayed.
        m_RecordedGeometryHolds.clear();
        m_RecordedLayoutHolds.clear();
        m_RecordedCommands.clear();
        m_UploadScratchCursor = 0;
        m_State = ERHICommandListState::Idle;
    }

    uint64_t RHICommandList::GetImmediateUploadWhileRecordingCount()
    {
        return g_ImmediateUploadWhileRecordingCount.load();
    }

    uint64_t RHICommandList::GetRecordedBindingSetHoldCount()
    {
        return g_RecordedBindingSetHoldCount.load();
    }

    bool RHICommandList::IsRecordingWindowOpenOnThisThread()
    {
        return t_OpenRecordingWindowCount != 0;
    }

    void RHICommandList::UpdateBuffer(RHIBuffer* buffer, const void* data, uint32_t offset, uint32_t size)
    {
        ME_ASSERT(buffer != nullptr, "UpdateBuffer requires a buffer");
        if (buffer == nullptr || data == nullptr || size == 0)
        {
            return;
        }

        if (IsImmediate())
        {
            buffer->UpdateSubresource(data, offset, size);
            return;
        }

        AssertCanIssueDrawCommand();
        if (m_State != ERHICommandListState::Recording)
        {
            // Refused and reported; a payload copy for a command that will never run is waste.
            return;
        }

        uint8_t* payload = AcquireUploadScratchBlock(m_UploadScratch, m_UploadScratchCursor, size);
        std::memcpy(payload, data, size);
        RecordOrExecute([buffer, payload, offset, size]() { buffer->UpdateSubresource(payload, offset, size); });
    }

    void RHICommandList::AssertCanIssueDrawCommand() const
    {
        if (IsImmediate() || m_State == ERHICommandListState::Recording)
        {
            return;
        }

        if (IsOffRoleCaller())
        {
            RefuseOffRoleCommand();
            return;
        }

        // Reported, not asserted. A command issued outside a window has no place in a recorded
        // stream, and taking the process down over it turns a mistimed frame into a lost session.
        // It is refused at the call site instead: the caller sees the line, the count moves, and
        // the frame that follows is still drawn.
        const uint64_t refused = g_CommandOutsideRecordingCount.fetch_add(1, std::memory_order_relaxed) + 1;
        // The first one, then one in a hundred: a frame that issues many has one cause, and a line
        // per command would bury the first.
        if (refused == 1 || refused % 100 == 0)
        {
            ME_LOG(
                LogRender,
                Error,
                "RHICommandList command refused: the list is {} while this topology records commands "
                "instead of running them immediately, so a command issued now has no recording to "
                "belong to. Open the window with BeginRecording() and hand the stream over with "
                "FinishRecording()/Replay(). {} command(s) refused so far; caller:{}",
                CommandListStateName(m_State),
                refused,
                DescribeRecordingCaller());
        }
    }

    void RHICommandList::RecordOrExecute(std::function<void()> command)
    {
        ME_ASSERT(static_cast<bool>(command), "RHICommandList recorded command must be callable");
        ME_ASSERT(m_RHI != nullptr, "RHICommandList requires an executing RHI");

        if (IsImmediate())
        {
            command();
            return;
        }

        AssertCanIssueDrawCommand();
        if (m_State != ERHICommandListState::Recording)
        {
            return;
        }
        m_RecordedCommands.push_back(std::move(command));
    }

    void RHICommandList::BeginRenderPass(const RHIRenderPassInfo& info)
    {
        RecordOrExecute([this, info]() { m_RHI->RHICmdBeginRenderPass(info); });
    }

    void RHICommandList::EndRenderPass()
    {
        RecordOrExecute([this]() { m_RHI->RHICmdEndRenderPass(); });
    }

    void RHICommandList::SetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState)
    {
        RecordOrExecute([this, pipelineState]() { m_RHI->RHICmdSetGraphicsPipelineState(pipelineState); });
    }

    void RHICommandList::SetShaderBindingSet(uint32_t setIndex, RHIShaderBindingSet* bindingSet)
    {
        RecordOrExecute([this, setIndex, bindingSet]() {
            m_RHI->RHICmdSetShaderBindingSet(setIndex, bindingSet);
        });
    }

    void RHICommandList::SetShaderBindingSet(uint32_t setIndex, const RHIShaderBindingSetRef& bindingSet)
    {
        if (!bindingSet)
        {
            SetShaderBindingSet(setIndex, static_cast<RHIShaderBindingSet*>(nullptr));
            return;
        }

        // Not routed through RecordOrExecute: only the recorded command needs the reference, and
        // only this overload knows it took one. An immediate list runs the lambda before the call
        // returns, so the shared copy is released straight away and nothing is counted.
        if (IsImmediate())
        {
            m_RHI->RHICmdSetShaderBindingSet(setIndex, bindingSet.get());
            return;
        }

        AssertCanIssueDrawCommand();
        if (m_State != ERHICommandListState::Recording)
        {
            return;
        }

        m_RecordedCommands.push_back(
            [this, setIndex, bindingSet]() {
                m_RHI->RHICmdSetShaderBindingSet(setIndex, bindingSet.get());
            });
        ++m_RecordedBindingSetHolds;
        g_RecordedBindingSetHoldCount.fetch_add(1, std::memory_order_relaxed);
    }

    void RHICommandList::Transition(const RHITextureTransitionInfo& transition)
    {
        RecordOrExecute([this, transition]() { m_RHI->RHICmdTransition(transition); });
    }

    void RHICommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool flipY)
    {
        RecordOrExecute([this, x, y, width, height, flipY]() {
            m_RHI->RHICmdSetViewport(x, y, width, height, flipY);
        });
    }

    void RHICommandList::SetViewport(
        uint32_t x,
        uint32_t y,
        uint32_t width,
        uint32_t height,
        RHIViewportConvention convention)
    {
        SetViewport(x, y, width, height, GetViewportFlipY(convention));
    }

    void RHICommandList::SetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot)
    {
        RecordOrExecute([this, vertexBuffer, slot]() { m_RHI->RHICmdSetVertexBuffer(vertexBuffer, slot); });
    }

    void RHICommandList::SetIndexBuffer(RHIBuffer* indexBuffer)
    {
        RecordOrExecute([this, indexBuffer]() { m_RHI->RHICmdSetIndexBuffer(indexBuffer); });
    }

    void RHICommandList::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
    {
        RecordOrExecute([this, indexCount, firstIndex, vertexOffset]() {
            m_RHI->RHICmdDrawIndexed(indexCount, firstIndex, vertexOffset);
        });
    }

    void RHICommandList::Draw(uint32_t vertexCount, uint32_t firstVertex)
    {
        RecordOrExecute([this, vertexCount, firstVertex]() { m_RHI->RHICmdDraw(vertexCount, firstVertex); });
    }

    void RHICommandList::GenerateMips(RHITexture* texture)
    {
        RecordOrExecute([this, texture]() { m_RHI->RHICmdGenerateMips(texture); });
    }

    void RHICommandList::BindPacketDescriptorSets(const MeshDrawPacket& packet)
    {
        RHIGraphicsPipelineState* pipelineState = packet.PipelineState.get();
        RHIPipelineLayout* pipelineLayout = nullptr;
        if (pipelineState)
        {
            if (auto* fallback = static_cast<RHIGraphicsPSOStateFallback*>(pipelineState))
            {
                pipelineLayout = fallback->GetDesc().PipelineLayout;
            }
        }

        if (pipelineLayout)
        {
            const uint32_t setCount = pipelineLayout->GetShaderBindingSetLayoutCount();
            for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
            {
                if (packet.ShaderBindingSets[setIndex])
                {
                    SetShaderBindingSet(setIndex, packet.ShaderBindingSets[setIndex]);
                }
            }
            return;
        }

        for (uint32_t setIndex = 0; setIndex < kMaxShaderBindingSets; ++setIndex)
        {
            if (packet.ShaderBindingSets[setIndex])
            {
                SetShaderBindingSet(setIndex, packet.ShaderBindingSets[setIndex]);
            }
        }
    }

    void RHICommandList::SubmitDrawBuffers(const MeshDrawPacket& packet)
    {
        // The draw ranges are the ones the producer recorded in the packet. They are deliberately
        // not read back from the GPU objects: a packet borrows its buffers for one frame, so asking
        // a borrowed object for its size is exactly the dereference a lifetime contract must make
        // impossible. A packet with no range has nothing to draw, and drawing it anyway would
        // submit an unbounded draw.
        //
        // A recorded stream outlives the packet that filled it - and outlives the frame that
        // recorded it when replay happens on another thread - so the geometry the stream names is
        // held here, before anything can be skipped. A skipped frame is exactly when the handle
        // would otherwise be released while its creation is still queued for the graphics role.
        if (!IsImmediate() && m_State == ERHICommandListState::Recording)
        {
            if (packet.VertexBufferRef)
            {
                m_RecordedGeometryHolds.push_back(packet.VertexBufferRef);
            }
            if (packet.IndexBufferRef)
            {
                m_RecordedGeometryHolds.push_back(packet.IndexBufferRef);
            }
            if (packet.VertexInputLayoutRef)
            {
                m_RecordedLayoutHolds.push_back(packet.VertexInputLayoutRef);
            }
        }

        if (packet.IndexBuffer)
        {
            if (packet.IndexCount == 0)
            {
                return;
            }

            // The layout is the vertex state the draw runs with: without a native object behind it
            // the backend has no vertex array to bind, and the draw names nothing. It is checked
            // before the buffers because a packet can only draw them through it, and a packet with
            // no layout at all is a producer that never bound one.
            if (!CanNameGeometry(packet.VertexInputLayout, "vertex input layout", packet.IndexCount))
            {
                return;
            }

            // A handle whose native object does not exist yet would name no data. Which frame can
            // name it is the frame that starts after the graphics role created it, so this one is
            // skipped rather than handed to the backend.
            if (!CanNameGeometry(packet.IndexBuffer, "index buffer", packet.IndexCount))
            {
                return;
            }
            if (packet.VertexBuffer != nullptr &&
                !CanNameGeometry(packet.VertexBuffer, "vertex buffer", packet.IndexCount))
            {
                return;
            }

            if (packet.VertexBuffer)
            {
                SetVertexBuffer(packet.VertexBuffer);
            }
            SetIndexBuffer(packet.IndexBuffer);
            DrawIndexed(packet.IndexCount, packet.FirstIndex, packet.VertexOffset);
            return;
        }

        if (packet.VertexBuffer == nullptr || packet.VertexCount == 0)
        {
            return;
        }

        if (!CanNameGeometry(packet.VertexInputLayout, "vertex input layout", packet.VertexCount))
        {
            return;
        }

        if (!CanNameGeometry(packet.VertexBuffer, "vertex buffer", packet.VertexCount))
        {
            return;
        }

        SetVertexBuffer(packet.VertexBuffer);
        Draw(packet.VertexCount, packet.FirstVertex);
    }

    void RHICommandList::SubmitMeshDrawPacket(const MeshDrawPacket& packet)
    {
        if (!packet.PipelineState)
        {
            return;
        }

        SetGraphicsPipelineState(packet.PipelineState.get());
        BindPacketDescriptorSets(packet);
        SubmitDrawBuffers(packet);
    }

    RHITextureRef RHICommandList::CreateTexture2D(const RHITextureCreateDesc& desc, const void* initialData)
    {
        ME_ASSERT(m_RHI != nullptr, "CreateTexture2D requires an executing RHI");
        return m_RHI->RHICreateTexture2D(desc, initialData);
    }

    RHIShaderResourceViewRef RHICommandList::CreateShaderResourceView(const RHITextureSRVDesc& desc)
    {
        ME_ASSERT(m_RHI != nullptr, "CreateShaderResourceView requires an executing RHI");
        return m_RHI->RHICreateShaderResourceView(desc);
    }

    RHIBufferRef RHICommandList::CreateBuffer(const RHIBufferCreateDesc& desc, const void* initialData)
    {
        ME_ASSERT(m_RHI != nullptr, "CreateBuffer requires an executing RHI");
        return m_RHI->RHICreateBuffer(desc, initialData);
    }

    RHIShaderRef RHICommandList::CreateShader(const RHIShaderCreateDesc& desc, std::string* outCompileLog)
    {
        ME_ASSERT(m_RHI != nullptr, "CreateShader requires an executing RHI");
        return m_RHI->RHICreateShader(desc, outCompileLog);
    }

    RHIShaderRef RHICommandList::CreateShader(
        const std::string& vertexSource,
        const std::string& fragmentSource,
        std::string* outCompileLog)
    {
        ME_ASSERT(m_RHI != nullptr, "CreateShader requires an executing RHI");
        return m_RHI->RHICreateShader(vertexSource, fragmentSource, outCompileLog);
    }

    RHIGraphicsPipelineStateRef RHICommandList::CreateGraphicsPipelineState(const RHIGraphicsPSODesc& desc)
    {
        ME_ASSERT(m_RHI != nullptr, "CreateGraphicsPipelineState requires an executing RHI");
        return m_RHI->RHICreateGraphicsPipelineState(desc);
    }

    RHIShaderBindingSetLayoutRef RHICommandList::CreateShaderBindingSetLayout(
        const std::vector<RHIShaderBindingSetLayoutEntry>& entries)
    {
        ME_ASSERT(m_RHI != nullptr, "CreateShaderBindingSetLayout requires an executing RHI");
        return m_RHI->RHICreateShaderBindingSetLayout(entries);
    }

    RHIPipelineLayoutRef RHICommandList::CreatePipelineLayout(
        const std::vector<RHIShaderBindingSetLayout*>& setLayouts)
    {
        ME_ASSERT(m_RHI != nullptr, "CreatePipelineLayout requires an executing RHI");
        return m_RHI->RHICreatePipelineLayout(setLayouts);
    }

    RHIShaderBindingSetRef RHICommandList::CreateShaderBindingSet(
        RHIShaderBindingSetLayout* layout,
        const std::vector<RHIShaderBinding>& resources)
    {
        ME_ASSERT(m_RHI != nullptr, "CreateShaderBindingSet requires an executing RHI");
        return m_RHI->RHICreateShaderBindingSet(layout, resources);
    }

    std::shared_ptr<RHIVertexInputLayout> RHICommandList::CreateVertexInputLayout(
        std::initializer_list<RHIVertexElement> elements)
    {
        ME_ASSERT(m_RHI != nullptr, "CreateVertexInputLayout requires an executing RHI");
        return m_RHI->RHICreateVertexInputLayout(elements);
    }
}
