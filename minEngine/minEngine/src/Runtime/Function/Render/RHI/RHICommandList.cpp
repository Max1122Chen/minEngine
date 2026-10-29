#include "RHICommandList.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"

namespace minEngine
{
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
        if (FrameThreading::HasInstance() && FrameThreading::Get().IsMulti()
            && TaskSystem::HasInstance() && TaskSystem::Get().AreFrameRolesRunning())
        {
            return false;
        }
        return true;
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
        ME_ASSERT(m_State == ERHICommandListState::Idle, "BeginRecording requires Idle");
        m_RecordedCommands.clear();
        m_State = ERHICommandListState::Recording;
    }

    void RHICommandList::FinishRecording()
    {
        if (IsImmediate())
        {
            return;
        }
        ME_ASSERT(m_State == ERHICommandListState::Recording, "FinishRecording requires Recording");
        m_State = ERHICommandListState::Finished;
    }

    void RHICommandList::Replay()
    {
        if (IsImmediate())
        {
            return;
        }
        ME_ASSERT(m_State == ERHICommandListState::Finished, "Replay requires Finished");
        ME_ASSERT(m_RHI != nullptr, "Replay requires an executing RHI");
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
        m_RecordedCommands.clear();
        m_State = ERHICommandListState::Idle;
    }

    void RHICommandList::AssertCanIssueDrawCommand() const
    {
        if (IsImmediate())
        {
            return;
        }
        ME_ASSERT(m_State == ERHICommandListState::Recording, "Draw commands require Recording when not Immediate");
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
        if (packet.IndexBuffer)
        {
            if (packet.IndexCount == 0)
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
