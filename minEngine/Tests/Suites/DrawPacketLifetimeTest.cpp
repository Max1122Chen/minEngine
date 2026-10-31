#include "DrawPacketLifetimeTest.h"

#include "Support/HeadlessTestRHI.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIDeferredCreate.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/SceneProxies/WidgetSceneProxy.h"
#include "Runtime/Function/UI/Font/TextMeshBuilder.h"

#include "doctest.h"

#include <atomic>
#include <mutex>
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
using minEngine::WidgetSceneProxy;

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

    /**
     * A layout handle whose native object already exists: the vertex state a real draw always
     * carries, so a case that is about the geometry does not also exercise the layout gate.
     */
    minEngine::RHIVertexInputLayoutRef MakeReadyLayout(HeadlessTestRHI& rhi)
    {
        return rhi.RHICreateVertexInputLayout({
            minEngine::RHIVertexElement("a_Position", minEngine::VertexElementType::Float3),
        });
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
    const minEngine::RHIVertexInputLayoutRef layout = MakeReadyLayout(rhi);

    MeshDrawCommand command;
    command.m_IndexBufferRef = indexBuffer;
    command.m_IndexCount = 36;
    command.m_VertexInputLayoutRef = layout;
    command.m_VertexInputLayout = layout.get();
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
    const minEngine::RHIVertexInputLayoutRef layout = MakeReadyLayout(rhi);

    MeshDrawCommand command;
    command.m_IndexBufferRef = indexBuffer;
    command.m_IndexCount = 36;
    command.m_VertexInputLayoutRef = layout;
    command.m_VertexInputLayout = layout.get();

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

    const minEngine::RHIVertexInputLayoutRef layout = MakeReadyLayout(rhi);
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
        command.m_VertexInputLayoutRef = layout;
        command.m_VertexInputLayout = layout.get();
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

namespace
{
    /**
     * A layout handle whose native object arrives with the graphics role, like a backend vertex
     * array object: it exists as a handle at once and as something drawable only after creation.
     */
    class DeferredVertexInputLayout final : public minEngine::RHIVertexInputLayout
    {
    public:
        DeferredVertexInputLayout()
        {
            m_Elements.emplace_back("a_Position", minEngine::VertexElementType::Float3);
        }

        const std::vector<minEngine::RHIVertexElement>& GetElements() const override { return m_Elements; }
        uint32_t GetStride() const override { return 12u; }

        bool CreateNativeResources() override
        {
            m_bNativeObject = true;
            return true;
        }
        bool IsReady() const override { return m_bNativeObject; }

    private:
        std::vector<minEngine::RHIVertexElement> m_Elements;
        bool m_bNativeObject = false;
    };
}

TEST_CASE("draw-packet-lifetime: a draw whose vertex layout is not materialized is not submitted [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    HeadlessTestRHI rhi;
    InstallDeferredBufferFactory(rhi);

    RHIBufferRef indexBuffer = rhi.RHICreateBuffer(MakeIndexBufferDesc(), nullptr);
    REQUIRE(indexBuffer != nullptr);

    std::shared_ptr<DeferredVertexInputLayout> layout = std::make_shared<DeferredVertexInputLayout>();
    REQUIRE(!layout->IsReady());

    MeshDrawCommand command;
    command.m_VertexInputLayoutRef = layout;
    command.m_VertexInputLayout = layout.get();
    command.m_IndexBufferRef = indexBuffer;
    command.m_IndexBuffer = indexBuffer.get();
    command.m_IndexCount = 36;

    MeshDrawPacket packet = MakePacket(rhi);
    packet.BindDrawCommand(command);
    REQUIRE(packet.VertexInputLayout == layout.get());
    REQUIRE(packet.VertexInputLayoutRef.get() == layout.get());

    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        rhi.RHIDrainDeferredCreates();
    }
    REQUIRE(indexBuffer->IsReady());

    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(true);

    // The geometry is ready but the vertex state it is interpreted through is not: the draw would
    // bind a vertex array object that does not exist yet, so this frame has to be skipped.
    cmdList.SubmitMeshDrawPacket(packet);
    CHECK(rhi.Draws.empty());
    CHECK(rhi.NamelessDraws == 0);

    layout->CreateNativeResources();
    cmdList.SubmitMeshDrawPacket(packet);
    REQUIRE(rhi.Draws.size() == 1u);
    CHECK(rhi.NamelessDraws == 0);
    CHECK(rhi.Draws[0].IndexBuffer == indexBuffer.get());

    threading.Shutdown();
}

TEST_CASE("draw-packet-lifetime: a recorded stream holds the vertex layout it names until it is replayed [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    HeadlessTestRHI rhi;
    InstallDeferredBufferFactory(rhi);

    RHIBufferRef indexBuffer = rhi.RHICreateBuffer(MakeIndexBufferDesc(), nullptr);
    REQUIRE(indexBuffer != nullptr);
    indexBuffer->CreateNativeResources();
    REQUIRE(indexBuffer->IsReady());

    minEngine::RHIVertexInputLayoutRef layout = rhi.RHICreateVertexInputLayout({
        minEngine::RHIVertexElement("a_Position", minEngine::VertexElementType::Float3),
    });
    REQUIRE(layout != nullptr);
    REQUIRE(layout->IsReady());
    const std::weak_ptr<minEngine::RHIVertexInputLayout> layoutWeak = layout;

    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(false);
    {
        // The packet is a temporary that dies with the recording call, exactly as a pass builds one
        // per frame. Only the stream names the layout from here on, and the stream is replayed
        // later, so the stream is what has to hold it.
        MeshDrawPacket packet = MakePacket(rhi);
        MeshDrawCommand command;
        command.m_VertexInputLayoutRef = layout;
        command.m_VertexInputLayout = layout.get();
        command.m_IndexBufferRef = indexBuffer;
        command.m_IndexBuffer = indexBuffer.get();
        command.m_IndexCount = 36;
        packet.BindDrawCommand(command);

        cmdList.BeginRecording();
        cmdList.SubmitMeshDrawPacket(packet);
        cmdList.FinishRecording();
    }
    layout.reset();

    CHECK_FALSE(layoutWeak.expired());

    cmdList.Replay();
    CHECK(layoutWeak.expired());
    CHECK(rhi.NamelessDraws == 0);

    threading.Shutdown();
}

namespace
{
    /**
     * Counts the one line that says a draw read its vertices through the layout the pipeline state
     * binds: the case is legitimate, and that line is what makes it visible in a real log.
     */
    class PipelineStateLayoutNoticeSink final : public minEngine::ILogSink
    {
    public:
        void Write(const minEngine::LogRecord& record) override
        {
            if (record.message.find("vertex input layout the graphics pipeline state binds")
                != std::string::npos)
            {
                Count.fetch_add(1, std::memory_order_relaxed);
            }
        }

        std::atomic<int> Count{ 0 };
    };

    PipelineStateLayoutNoticeSink& GetLayoutNoticeSink()
    {
        static PipelineStateLayoutNoticeSink s_Sink;
        return s_Sink;
    }

    /** Registers the notice sink once per process; the log system owns the reference. */
    void EnsureLayoutNoticeSinkInstalled()
    {
        static std::once_flag s_Once;
        std::call_once(s_Once, []() {
            minEngine::LogSystem::AddSink(std::shared_ptr<minEngine::ILogSink>(
                &GetLayoutNoticeSink(),
                [](minEngine::ILogSink*) {
                    // Process-lifetime probe: the log system keeps the reference.
                }));
        });
        // The notice is informational, so the channel has to let it through for this to read it.
        minEngine::LogSystem::SetChannelSeverity(minEngine::LogRender, minEngine::LogSeverity::Info);
    }

    /** A pipeline state that binds the given layout when it is applied. */
    minEngine::RHIGraphicsPipelineStateRef MakePipelineStateWithLayout(
        HeadlessTestRHI& rhi,
        minEngine::RHIVertexInputLayout* layout)
    {
        RHIGraphicsPSODesc desc;
        desc.VertexInputLayout = layout;
        return rhi.RHICreateGraphicsPipelineState(desc);
    }

    /** A quad draw of six indices whose geometry is ready and whose packet carries no layout. */
    MeshDrawPacket MakeQuadPacketWithoutLayout(const RHIBufferRef& indexBuffer)
    {
        MeshDrawPacket packet;
        packet.IndexBufferRef = indexBuffer;
        packet.IndexBuffer = indexBuffer.get();
        packet.IndexCount = 6u;
        return packet;
    }
}

TEST_CASE("draw-packet-lifetime: a draw whose layout comes from the pipeline state is submitted [smoke]")
{
    EnsureLayoutNoticeSinkInstalled();
    PipelineStateLayoutNoticeSink& notice = GetLayoutNoticeSink();
    const int noticesBefore = notice.Count.load(std::memory_order_relaxed);

    HeadlessTestRHI rhi;
    InstallDeferredBufferFactory(rhi);

    RHIBufferRef indexBuffer = rhi.RHICreateBuffer(MakeIndexBufferDesc(), nullptr);
    REQUIRE(indexBuffer != nullptr);
    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        rhi.RHIDrainDeferredCreates();
    }
    REQUIRE(indexBuffer->IsReady());

    const minEngine::RHIVertexInputLayoutRef layout = MakeReadyLayout(rhi);
    REQUIRE(layout != nullptr);
    REQUIRE(layout->IsReady());

    // A pass that owns exactly one vertex state keeps it on the pipeline state instead of copying
    // it into every packet. The draw is still complete: the state binds that layout when it is
    // applied, so nothing about it is missing.
    MeshDrawPacket packet = MakeQuadPacketWithoutLayout(indexBuffer);
    packet.PipelineState = MakePipelineStateWithLayout(rhi, layout.get());
    REQUIRE(packet.VertexInputLayout == nullptr);
    REQUIRE(packet.VertexInputLayoutRef == nullptr);

    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(true);
    cmdList.SubmitMeshDrawPacket(packet);

    CHECK(rhi.Draws.size() == 1u);
    CHECK(rhi.NamelessDraws == 0);
    CHECK(notice.Count.load(std::memory_order_relaxed) > noticesBefore);
}

TEST_CASE("draw-packet-lifetime: a layout the pipeline state names but has not materialized is not submitted [smoke]")
{
    HeadlessTestRHI rhi;
    InstallDeferredBufferFactory(rhi);

    RHIBufferRef indexBuffer = rhi.RHICreateBuffer(MakeIndexBufferDesc(), nullptr);
    REQUIRE(indexBuffer != nullptr);
    indexBuffer->CreateNativeResources();
    REQUIRE(indexBuffer->IsReady());

    const std::shared_ptr<DeferredVertexInputLayout> layout =
        std::make_shared<DeferredVertexInputLayout>();
    REQUIRE(!layout->IsReady());

    MeshDrawPacket packet = MakeQuadPacketWithoutLayout(indexBuffer);
    packet.PipelineState = MakePipelineStateWithLayout(rhi, layout.get());
    REQUIRE(packet.VertexInputLayout == nullptr);

    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(true);

    // Moving the layout to the pipeline state does not make an unmaterialized one drawable: the
    // vertex array the state would bind does not exist yet either.
    cmdList.SubmitMeshDrawPacket(packet);
    CHECK(rhi.Draws.empty());
    CHECK(rhi.NamelessDraws == 0);

    layout->CreateNativeResources();
    cmdList.SubmitMeshDrawPacket(packet);
    REQUIRE(rhi.Draws.size() == 1u);
    CHECK(rhi.NamelessDraws == 0);
}

TEST_CASE("draw-packet-lifetime: a draw that names no layout anywhere is not submitted [smoke]")
{
    HeadlessTestRHI rhi;
    InstallDeferredBufferFactory(rhi);

    RHIBufferRef indexBuffer = rhi.RHICreateBuffer(MakeIndexBufferDesc(), nullptr);
    REQUIRE(indexBuffer != nullptr);
    indexBuffer->CreateNativeResources();
    REQUIRE(indexBuffer->IsReady());

    // Nothing binds a vertex layout: no layout on the packet, and a state built without one.
    MeshDrawPacket packet = MakeQuadPacketWithoutLayout(indexBuffer);
    packet.PipelineState = rhi.RHICreateGraphicsPipelineState(RHIGraphicsPSODesc{});

    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(true);
    cmdList.SubmitMeshDrawPacket(packet);

    // A draw of that shape reads no vertex data at all, whatever the state does with it.
    CHECK(rhi.Draws.empty());
    CHECK(rhi.NamelessDraws == 0);
}

TEST_CASE("draw-packet-lifetime: a text widget's packet names the glyph layout it draws through [smoke]")
{
    HeadlessTestRHI rhi;
    InstallDeferredBufferFactory(rhi);

    RHIBufferCreateDesc vertexDesc;
    vertexDesc.Usage = RHIBufferUsage::Vertex;
    vertexDesc.ByteSize = 4u * 32u;
    vertexDesc.ElementCount = 4u;
    const RHIBufferRef vertexBuffer = rhi.RHICreateBuffer(vertexDesc, nullptr);
    const RHIBufferRef indexBuffer = rhi.RHICreateBuffer(MakeIndexBufferDesc(), nullptr);
    REQUIRE(vertexBuffer != nullptr);
    REQUIRE(indexBuffer != nullptr);
    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        rhi.RHIDrainDeferredCreates();
    }
    REQUIRE(vertexBuffer->IsReady());
    REQUIRE(indexBuffer->IsReady());

    // Glyph geometry reads its vertices through one layout the text builder shares, and the builder
    // keeps it as a handle. That handle is what a text graphic hands to its widget, so the proxy -
    // and every draw recorded from it - names a layout that outlives the text that produced it.
    // The builder's own handle stays inside the engine module, so this case binds a handle of the
    // same kind and pins the hand-over: what a widget binds is what its draws name.
    REQUIRE(minEngine::TextMeshBuilder::GetOrCreateVertexInputLayout(rhi) != nullptr);
    CHECK(minEngine::TextMeshBuilder::HasSharedLayout());
    const minEngine::RHIVertexInputLayoutRef sharedLayout = rhi.RHICreateVertexInputLayout({
        minEngine::RHIVertexElement("a_Position", minEngine::VertexElementType::Float3),
        minEngine::RHIVertexElement("a_TexCoord", minEngine::VertexElementType::Float2),
    });
    REQUIRE(sharedLayout != nullptr);
    CHECK(minEngine::GetDeferredUse(sharedLayout.get()) == minEngine::RHIDeferredUse::Usable);

    MeshDrawPacket packet;
    {
        WidgetSceneProxy proxy;
        proxy.SetMeshResources(vertexBuffer, sharedLayout, indexBuffer, 6u, 4u);
        REQUIRE(proxy.m_VertexInputLayoutRef != nullptr);
        CHECK(proxy.m_VertexInputLayout == sharedLayout.get());

        // What the ScreenUI queue copies out of the proxy, and what a pass turns into a packet.
        MeshDrawCommand command;
        command.m_VertexBufferRef = proxy.m_VertexBufferRef;
        command.m_IndexBufferRef = proxy.m_IndexBufferRef;
        command.m_VertexBuffer = proxy.m_VertexBuffer;
        command.m_IndexBuffer = proxy.m_IndexBuffer;
        command.m_VertexInputLayoutRef = proxy.m_VertexInputLayoutRef;
        command.m_VertexInputLayout = proxy.m_VertexInputLayout;
        command.m_IndexCount = proxy.m_IndexCount;
        REQUIRE(command.m_VertexInputLayoutRef != nullptr);
        CHECK(minEngine::GetDeferredUse(command.m_VertexInputLayout)
            == minEngine::RHIDeferredUse::Usable);
        packet.BindDrawCommand(command);
    }

    // The state this draw is recorded with binds no layout of its own: the layout the packet names
    // is the only one the draw can read its vertices through.
    packet.PipelineState = rhi.RHICreateGraphicsPipelineState(RHIGraphicsPSODesc{});
    REQUIRE(packet.VertexInputLayoutRef != nullptr);
    REQUIRE(packet.VertexInputLayout != nullptr);
    CHECK(minEngine::GetDeferredUse(packet.VertexInputLayout) == minEngine::RHIDeferredUse::Usable);

    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(true);
    cmdList.SubmitMeshDrawPacket(packet);

    CHECK(rhi.Draws.size() == 1u);
    CHECK(rhi.NamelessDraws == 0);

    // The layout is a process-level handle: giving it back here leaves the next suite a clean one.
    packet = {};
    minEngine::TextMeshBuilder::ReleaseSharedLayout();
    CHECK_FALSE(minEngine::TextMeshBuilder::HasSharedLayout());
}


