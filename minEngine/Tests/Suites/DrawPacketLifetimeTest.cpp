#include "DrawPacketLifetimeTest.h"

#include "Support/HeadlessTestRHI.h"

#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"

#include "doctest.h"

#include <utility>

using minEngine::FrameThreading;
using minEngine::FrameThreadKind;
using minEngine::FrameThreadMode;
using minEngine::MeshDrawCommand;
using minEngine::MeshDrawPacket;
using minEngine::RHIBuffer;
using minEngine::RHIBufferCreateDesc;
using minEngine::RHIBufferRef;
using minEngine::RHIBufferUsage;
using minEngine::ScopedFrameThreadKind;
using minEngine::RHICommandList;
using minEngine::RHIGraphicsPSODesc;
using minEngine::RHIShaderBindingSet;
using minEngine::RHIShaderBindingSetRef;
using minEngine::Testing::GpuHandleProbe;
using minEngine::Testing::HeadlessTestRHI;
using minEngine::Testing::ProbeShaderBindingSet;
using minEngine::Testing::ProbeShaderBindingSetLayout;

namespace
{
    constexpr uint32_t kMaterialSlot = minEngine::EngineShaderBindings::kSetMaterial;

    /** A packet whose pipeline state exists but names no layout, so every filled set is bound. */
    MeshDrawPacket MakePacket(HeadlessTestRHI& rhi)
    {
        MeshDrawPacket packet;
        RHIGraphicsPSODesc desc;
        packet.PipelineState = rhi.RHICreateGraphicsPipelineState(desc);
        return packet;
    }

    /**
     * A geometry handle whose native object is a flag: not ready until the graphics role has run
     * its creation, exactly like a backend buffer created off that role.
     */
    class ProbeGeometryBuffer final : public RHIBuffer
    {
    public:
        explicit ProbeGeometryBuffer(const RHIBufferCreateDesc& desc)
            : m_Desc(desc)
        {
        }

        const RHIBufferCreateDesc& GetDesc() const override { return m_Desc; }
        void UpdateSubresource(const void*, uint32_t, uint32_t) override {}
        bool CreateNativeResources() override
        {
            m_bHasNativeObject = true;
            return true;
        }
        bool IsReady() const override { return m_bHasNativeObject; }

    private:
        RHIBufferCreateDesc m_Desc;
        bool m_bHasNativeObject = false;
    };

    RHIBufferCreateDesc MakeIndexBufferDesc()
    {
        RHIBufferCreateDesc desc;
        desc.Usage = RHIBufferUsage::Index;
        desc.ByteSize = 36u * sizeof(uint32_t);
        desc.ElementCount = 36u;
        return desc;
    }

    /** Routes every buffer this device hands out through the deferred-create policy. */
    void InstallDeferredBufferFactory(HeadlessTestRHI& rhi)
    {
        rhi.CreateBufferHook = [&rhi](const RHIBufferCreateDesc& desc, const void*) {
            return rhi.ScheduleNativeCreate(
                std::make_shared<ProbeGeometryBuffer>(desc),
                "probe geometry buffer");
        };
    }
}

TEST_CASE("draw-packet-lifetime: a packet keeps the material set it names alive until it is replayed [smoke]")
{
    const std::shared_ptr<GpuHandleProbe> probe = std::make_shared<GpuHandleProbe>();
    HeadlessTestRHI rhi(probe);
    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(false);

    ProbeShaderBindingSetLayout layout({});
    RHIShaderBindingSetRef materialSet = rhi.RHICreateShaderBindingSet(&layout, {});
    REQUIRE(materialSet != nullptr);
    // Read once while the set is certainly alive; after the producer replaces it, only the address
    // may be compared.
    CHECK(static_cast<ProbeShaderBindingSet*>(materialSet.get())->GetSerial() == 1);
    RHIShaderBindingSet* const firstAddress = materialSet.get();

    MeshDrawPacket packet = MakePacket(rhi);
    // Submission fills the material slot from the proxy: the packet owns the set from here on.
    packet.ShaderBindingSets[kMaterialSlot] = materialSet;

    cmdList.BeginRecording();
    cmdList.SubmitMeshDrawPacket(packet);
    cmdList.FinishRecording();

    // The material compiled again: the proxy drops the set it built before and builds another one.
    // That is the moment the old code left the packet naming a destroyed object.
    materialSet.reset();
    RHIShaderBindingSetRef rebuilt = rhi.RHICreateShaderBindingSet(&layout, {});

    // The packet still names the first set, so the first set has to still exist: a recorded stream
    // that runs after this point reads it.
    CHECK(probe->HandlesAlive.load() == 2);
    CHECK(packet.ShaderBindingSets[kMaterialSlot].get() == firstAddress);

    cmdList.Replay();
    REQUIRE(rhi.BoundSets.size() == 1u);
    CHECK(rhi.BoundSets[0].first == kMaterialSlot);
    CHECK(rhi.BoundSets[0].second == firstAddress);
    CHECK(rhi.NullSetBinds == 0);

    // Replay handed the stream back: the packet holds the set that was recorded.
    CHECK(probe->HandlesAlive.load() == 2);
    packet = MeshDrawPacket{};
    CHECK(probe->HandlesAlive.load() == 1);

    // The rebuild's own set is the last one; both native objects are then destroyed by the drain.
    rebuilt.reset();
    CHECK(probe->HandlesAlive.load() == 0);
    CHECK(rhi.RHIDrainAllDeferredDestroys() == 2);
    CHECK(probe->NativesDestroyed.load() == 2);
    CHECK(probe->NativesRefused.load() == 0);
}

TEST_CASE("draw-packet-lifetime: a recorded set is held until the stream is replayed [smoke]")
{
    const std::shared_ptr<GpuHandleProbe> probe = std::make_shared<GpuHandleProbe>();
    HeadlessTestRHI rhi(probe);
    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(false);

    ProbeShaderBindingSetLayout layout({});
    RHIShaderBindingSetRef materialSet = rhi.RHICreateShaderBindingSet(&layout, {});
    REQUIRE(materialSet != nullptr);
    RHIShaderBindingSet* const firstAddress = materialSet.get();

    const uint64_t holdsBefore = RHICommandList::GetRecordedBindingSetHoldCount();
    cmdList.BeginRecording();
    cmdList.SetShaderBindingSet(kMaterialSlot, materialSet);
    CHECK(RHICommandList::GetRecordedBindingSetHoldCount() == holdsBefore + 1);
    cmdList.FinishRecording();

    // The producer drops its own reference first: only the stream keeps the set alive now.
    materialSet.reset();
    CHECK(probe->HandlesAlive.load() == 1);

    cmdList.Replay();
    REQUIRE(rhi.BoundSets.size() == 1u);
    CHECK(rhi.BoundSets[0].second == firstAddress);
    CHECK(rhi.NullSetBinds == 0);

    // The stream gave the reference back when it was replayed and reset.
    CHECK(RHICommandList::GetRecordedBindingSetHoldCount() == holdsBefore);
    CHECK(probe->HandlesAlive.load() == 0);

    // An immediate list runs the bind before the call returns, so it retains nothing: the count
    // does not move and the set is not what the caller's next line is waiting on.
    RHICommandList immediate(&rhi);
    immediate.SetImmediateOverride(true);
    RHIShaderBindingSetRef immediateSet = rhi.RHICreateShaderBindingSet(&layout, {});
    REQUIRE(immediateSet != nullptr);
    immediate.SetShaderBindingSet(kMaterialSlot, immediateSet);
    CHECK(RHICommandList::GetRecordedBindingSetHoldCount() == holdsBefore);
    CHECK(rhi.BoundSets.size() == 2u);
    CHECK(rhi.BoundSets[1].second == immediateSet.get());
    immediateSet.reset();

    CHECK(rhi.RHIDrainAllDeferredDestroys() == 2);
    CHECK(probe->NativesDestroyed.load() == 2);
    CHECK(probe->NativesRefused.load() == 0);
}

TEST_CASE("draw-packet-lifetime: the geometry a packet names is still built after its producer lets go [smoke]")
{
    // The topology where a buffer cannot be created by the caller: the handle comes back complete
    // and its native object waits for the graphics role.
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    HeadlessTestRHI rhi;
    InstallDeferredBufferFactory(rhi);

    MeshDrawPacket packet = MakePacket(rhi);

    RHIBufferRef indexBuffer = rhi.RHICreateBuffer(MakeIndexBufferDesc(), nullptr);
    REQUIRE(indexBuffer != nullptr);
    CHECK(!indexBuffer->IsReady());
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 1);

    MeshDrawCommand command;
    command.m_IndexBufferRef = indexBuffer;
    command.m_IndexCount = 36;
    packet.BindDrawCommand(command);

    // The producer hands the geometry to the frame and stops holding it. What the packet names is
    // what the frame draws, so the packet is the last owner of that handle from here on - or the
    // request has nobody left to ask for it and the frame-start drain skips it as unwanted.
    indexBuffer.reset();

    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        rhi.RHIDrainDeferredCreates();
    }

    CHECK(rhi.GetDeferredCreateQueue().GetNotNeededCount() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetNotNeededCount("probe geometry buffer") == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetCreatedCount() == 1);
    REQUIRE(packet.IndexBuffer != nullptr);

    // What the packet held was created, so the draw of it names data rather than nothing.
    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(true);
    cmdList.SubmitMeshDrawPacket(packet);
    REQUIRE(rhi.Draws.size() == 1u);
    CHECK(rhi.NamelessDraws == 0);
    CHECK(rhi.Draws[0].IndexBuffer == packet.IndexBuffer);

    threading.Shutdown();
}

TEST_CASE("draw-packet-lifetime: a draw whose geometry has no native object is not submitted [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    HeadlessTestRHI rhi;
    InstallDeferredBufferFactory(rhi);

    RHIBufferRef indexBuffer = rhi.RHICreateBuffer(MakeIndexBufferDesc(), nullptr);
    REQUIRE(indexBuffer != nullptr);
    REQUIRE(!indexBuffer->IsReady());

    MeshDrawCommand command;
    command.m_IndexBufferRef = indexBuffer;
    command.m_IndexCount = 36;

    MeshDrawPacket packet = MakePacket(rhi);
    packet.BindDrawCommand(command);

    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(true);

    // The handle exists and is held; its native object waits for the frame start that has not run
    // yet. Submitting the draw now would name no data, so the submission is what has to wait.
    cmdList.SubmitMeshDrawPacket(packet);
    CHECK(rhi.Draws.empty());
    CHECK(rhi.NamelessDraws == 0);

    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        rhi.RHIDrainDeferredCreates();
    }
    REQUIRE(indexBuffer->IsReady());

    // The frame after the creation names the object it waited for.
    cmdList.SubmitMeshDrawPacket(packet);
    REQUIRE(rhi.Draws.size() == 1u);
    CHECK(rhi.NamelessDraws == 0);
    CHECK(rhi.Draws[0].bIndexed);
    CHECK(rhi.Draws[0].Count == 36u);
    CHECK(rhi.Draws[0].IndexBuffer == indexBuffer.get());

    threading.Shutdown();
}
TEST_CASE("draw-packet-lifetime: a frame that cannot name its geometry keeps the request alive for the next one [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    HeadlessTestRHI rhi;
    InstallDeferredBufferFactory(rhi);

    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(false);

    RHIBufferRef indexBuffer = rhi.RHICreateBuffer(MakeIndexBufferDesc(), nullptr);
    REQUIRE(indexBuffer != nullptr);
    REQUIRE(!indexBuffer->IsReady());

    // The packet is a temporary that dies with the recording call, exactly as a pass builds one per
    // frame. Only the stream names the geometry from here on, and the stream is replayed later - so
    // the stream is what has to hold it, or the request is dropped as unwanted before the frame
    // that waits for it can run.
    {
        MeshDrawPacket packet = MakePacket(rhi);
        MeshDrawCommand command;
        command.m_IndexBufferRef = indexBuffer;
        command.m_IndexCount = 36;
        packet.BindDrawCommand(command);

        cmdList.BeginRecording();
        cmdList.SubmitMeshDrawPacket(packet);
        cmdList.FinishRecording();
    }
    indexBuffer.reset();

    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        rhi.RHIDrainDeferredCreates();
    }

    // The frame that could not name it submitted nothing, and the request it was waiting for was
    // not dropped while it waited.
    CHECK(rhi.Draws.empty());
    CHECK(rhi.NamelessDraws == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetNotNeededCount() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetCreatedCount() == 1);

    cmdList.Replay();
    CHECK(rhi.NamelessDraws == 0);

    threading.Shutdown();
}
