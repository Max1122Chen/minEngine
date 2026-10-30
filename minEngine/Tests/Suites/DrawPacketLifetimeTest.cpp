#include "DrawPacketLifetimeTest.h"

#include "Support/HeadlessTestRHI.h"

#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"

#include "doctest.h"

#include <utility>

using minEngine::MeshDrawPacket;
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
