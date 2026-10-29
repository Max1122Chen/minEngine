#include "RHIDeferredDestroyTest.h"

#include "Runtime/Core/Log/LogConsole.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHIRenderPass.h"

#include "doctest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using minEngine::FrameThreadMode;
using minEngine::FrameThreading;
using minEngine::PostNativeDestroy;
using minEngine::RHIBuffer;
using minEngine::RHIBufferCreateDesc;
using minEngine::RHIDeferredDestroyQueue;
using minEngine::RHIGraphicsPSODesc;
using minEngine::RHIGraphicsPipelineState;
using minEngine::RHIPipelineLayout;
using minEngine::RHIRenderPassInfo;
using minEngine::RHISampler;
using minEngine::RHISamplerDesc;
using minEngine::RHIShader;
using minEngine::RHIShaderBinding;
using minEngine::RHIShaderBindingSet;
using minEngine::RHIShaderBindingSetLayout;
using minEngine::RHIShaderBindingSetLayoutEntry;
using minEngine::RHIShaderCreateDesc;
using minEngine::RHIShaderResourceView;
using minEngine::RHITexture;
using minEngine::RHITextureCreateDesc;
using minEngine::RHITextureSRVDesc;
using minEngine::RHITextureTransitionInfo;
using minEngine::RHIVertexElement;
using minEngine::RHIVertexInputLayout;
using minEngine::TaskSystem;

namespace
{
    /**
     * Stands in for a backend RHI: the tests only need the queue the base hands out, so every
     * resource factory stays empty and the device-facing commands do nothing.
     */
    class HeadlessRHI final : public minEngine::RHI
    {
    public:
        void Initialize() override {}
        void Shutdown() override {}

        std::shared_ptr<RHITexture> RHICreateTexture2D(
            const RHITextureCreateDesc&,
            const void*) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIShaderResourceView> RHICreateShaderResourceView(
            const RHITextureSRVDesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIBuffer> RHICreateBuffer(const RHIBufferCreateDesc&, const void*) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIShader> RHICreateShader(
            const RHIShaderCreateDesc&,
            std::string*) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIShader> RHICreateShader(
            const std::string&,
            const std::string&,
            std::string*) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIGraphicsPipelineState> RHICreateGraphicsPipelineState(
            const RHIGraphicsPSODesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIShaderBindingSetLayout> RHICreateShaderBindingSetLayout(
            const std::vector<RHIShaderBindingSetLayoutEntry>&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIPipelineLayout> RHICreatePipelineLayout(
            const std::vector<RHIShaderBindingSetLayout*>&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIShaderBindingSet> RHICreateShaderBindingSet(
            RHIShaderBindingSetLayout*,
            const std::vector<RHIShaderBinding>&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHISampler> RHIGetOrCreateSampler(const RHISamplerDesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIVertexInputLayout> RHICreateVertexInputLayout(
            std::initializer_list<RHIVertexElement>) override
        {
            return nullptr;
        }

        void RHICmdBeginRenderPass(const RHIRenderPassInfo&) override {}
        void RHICmdEndRenderPass() override {}
        void RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState*) override {}
        void RHICmdSetShaderBindingSet(uint32_t, RHIShaderBindingSet*) override {}
        void RHICmdTransition(const RHITextureTransitionInfo&) override {}
        void RHICmdSetViewport(uint32_t, uint32_t, uint32_t, uint32_t, bool) override {}
        void RHICmdSetVertexBuffer(RHIBuffer*, uint32_t) override {}
        void RHICmdSetIndexBuffer(RHIBuffer*) override {}
        void RHICmdDrawIndexed(uint32_t, uint32_t, int32_t) override {}
        void RHICmdDraw(uint32_t, uint32_t) override {}
        void RHICmdGenerateMips(RHITexture*) override {}
        void RHISetBackbufferClearColor(const minEngine::Math::Vector3&) override {}
        void RHIClearBackbuffer() override {}
        void RHIPresent() override {}
    };

    /** What the fake native object records about its own destruction. */
    struct DestroyProbe
    {
        std::atomic<int> Destroyed{ 0 };
        std::atomic<int> DestroyAttempts{ 0 };
        std::atomic<std::thread::id> ThreadOfDestroy{};
        /** Stands in for "this moment cannot run the destroy call" (no graphics context). */
        std::atomic<bool> bCanDestroyNow{ true };
    };

    /**
     * A handle whose native object is a counter. It takes the same path a backend takes: the
     * destructor posts through the global entry point, and the posted callback is what counts as
     * the native object dying.
     */
    class ProbeBuffer final : public RHIBuffer
    {
    public:
        ProbeBuffer(
            const RHIBufferCreateDesc& desc,
            std::shared_ptr<DestroyProbe> probe,
            std::function<void()> onDestroy = {})
            : m_Desc(desc)
            , m_Probe(std::move(probe))
            , m_OnDestroy(std::move(onDestroy))
        {
        }

        ~ProbeBuffer() override
        {
            std::shared_ptr<DestroyProbe> probe = m_Probe;
            std::function<void()> onDestroy = m_OnDestroy;
            PostNativeDestroy(
                [probe, onDestroy]() {
                    // Runs before the probe is updated: a nested release posted from here has to
                    // be stamped against the batch that is being committed right now.
                    if (onDestroy)
                    {
                        onDestroy();
                    }
                    probe->DestroyAttempts.fetch_add(1, std::memory_order_relaxed);
                    if (!probe->bCanDestroyNow.load(std::memory_order_relaxed))
                    {
                        return false;
                    }
                    probe->ThreadOfDestroy.store(std::this_thread::get_id(), std::memory_order_release);
                    probe->Destroyed.fetch_add(1, std::memory_order_relaxed);
                    return true;
                },
                "probe buffer");
        }

        ProbeBuffer(const ProbeBuffer&) = delete;
        ProbeBuffer& operator=(const ProbeBuffer&) = delete;

        const RHIBufferCreateDesc& GetDesc() const override { return m_Desc; }
        void UpdateSubresource(const void*, uint32_t, uint32_t) override {}

    private:
        RHIBufferCreateDesc m_Desc;
        std::shared_ptr<DestroyProbe> m_Probe;
        std::function<void()> m_OnDestroy;
    };

    RHIBufferCreateDesc MakeProbeBufferDesc()
    {
        RHIBufferCreateDesc desc;
        desc.Usage = minEngine::RHIBufferUsage::Vertex;
        desc.ByteSize = 64;
        desc.Stride = 16;
        desc.ElementCount = 4;
        return desc;
    }

    std::shared_ptr<RHIBuffer> MakeProbeBuffer(
        const std::shared_ptr<DestroyProbe>& probe,
        std::function<void()> onDestroy = {})
    {
        return std::make_shared<ProbeBuffer>(
            MakeProbeBufferDesc(),
            probe,
            std::move(onDestroy));
    }
}

TEST_CASE("rhi-deferred-destroy: releasing the last reference alone does not destroy the native object [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();

    {
        std::shared_ptr<RHIBuffer> buffer = MakeProbeBuffer(probe);
        CHECK(buffer != nullptr);
    }

    // The releasing thread did not touch the native object; it handed the handle over instead, and
    // nothing was submitted to justify destroying it.
    CHECK(probe->Destroyed.load() == 0);
    CHECK(probe->DestroyAttempts.load() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetRetiredCount() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetDestroyedCount() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetCommittedBatchCount() == 0);
}

TEST_CASE("rhi-deferred-destroy: the native object dies at the tail of the batch that carried it [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();

    {
        std::shared_ptr<RHIBuffer> buffer = MakeProbeBuffer(probe);
    }

    // A handle retired before a batch is committed was recorded into that batch, so submitting the
    // batch is exactly the moment it becomes deletable - no frame count is involved.
    CHECK(rhi.RHISubmitBatchCommitted() == 1);
    CHECK(probe->Destroyed.load() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetCommittedBatchCount() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetDestroyedCount() == 1);

    // A further batch has nothing of its own to destroy.
    CHECK(rhi.RHISubmitBatchCommitted() == 0);
    CHECK(probe->Destroyed.load() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetCommittedBatchCount() == 2);
}

TEST_CASE("rhi-deferred-destroy: a handle retired after a batch was committed waits for the next one [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();

    CHECK(rhi.RHISubmitBatchCommitted() == 0);

    {
        std::shared_ptr<RHIBuffer> buffer = MakeProbeBuffer(probe);
    }

    // Retired between batches: no committed batch can have carried it, so it is not destroyed by
    // one that already happened.
    CHECK(probe->Destroyed.load() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 1);

    CHECK(rhi.RHISubmitBatchCommitted() == 1);
    CHECK(probe->Destroyed.load() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetDestroyedCount() == 1);
}

TEST_CASE("rhi-deferred-destroy: the destroy runs on the RHI role, not on the thread that dropped the handle [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);

    HeadlessRHI rhi;
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();
    {
        std::shared_ptr<RHIBuffer> buffer = MakeProbeBuffer(probe);
    }

    // The game role is not the RHI role: committing there would destroy on a thread that owns
    // neither the graphics context nor the frame being recorded, so it is refused and counted.
    CHECK(rhi.RHISubmitBatchCommitted() == 0);
    CHECK(probe->Destroyed.load() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetRefusedDrainCount() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetCommittedBatchCount() == 0);

    std::atomic<bool> rhiRoleHeld{ false };
    std::atomic<std::thread::id> rhiThreadId{};
    tasks.StartFrameRoles(
        [](float) {},
        [&rhi, &rhiRoleHeld, &rhiThreadId]() {
            rhiRoleHeld.store(FrameThreading::Get().IsInRHIThread(), std::memory_order_relaxed);
            rhiThreadId.store(std::this_thread::get_id(), std::memory_order_release);
            rhi.RHISubmitBatchCommitted();
        },
        /*combineGraphicsThreads=*/true);

    tasks.SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);
    CHECK(probe->Destroyed.load() == 1);
    CHECK(rhiRoleHeld.load());
    CHECK(probe->ThreadOfDestroy.load() == rhiThreadId.load());
    CHECK(probe->ThreadOfDestroy.load() != std::this_thread::get_id());

    tasks.Shutdown();
    threading.Shutdown();
}

TEST_CASE("rhi-deferred-destroy: one batch tail destroys every handle it carried, each exactly once [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();
    constexpr int kBufferCount = 64;

    std::vector<std::shared_ptr<RHIBuffer>> buffers;
    buffers.reserve(kBufferCount);
    for (int index = 0; index < kBufferCount; ++index)
    {
        buffers.push_back(MakeProbeBuffer(probe));
    }
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 0);
    CHECK(probe->Destroyed.load() == 0);

    buffers.clear();
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == kBufferCount);
    CHECK(probe->Destroyed.load() == 0);

    // One batch carried all of them out, so one tail retires all of them.
    CHECK(rhi.RHISubmitBatchCommitted() == kBufferCount);
    CHECK(probe->Destroyed.load() == kBufferCount);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetDestroyedCount() == kBufferCount);

    CHECK(rhi.RHISubmitBatchCommitted() == 0);
    CHECK(probe->Destroyed.load() == kBufferCount);
}

TEST_CASE("rhi-deferred-destroy: a handle that cannot be destroyed at its tail stays queued [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();
    probe->bCanDestroyNow.store(false);

    {
        std::shared_ptr<RHIBuffer> buffer = MakeProbeBuffer(probe);
    }

    rhi.RHISubmitBatchCommitted();
    rhi.RHISubmitBatchCommitted();

    // Reported, not dropped: the handle is still owned by the queue and its counter untouched.
    CHECK(probe->Destroyed.load() == 0);
    CHECK(probe->DestroyAttempts.load() >= 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetDeferredCount() >= 1);

    probe->bCanDestroyNow.store(true);
    CHECK(rhi.RHISubmitBatchCommitted() == 1);
    CHECK(probe->Destroyed.load() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 0);
}

TEST_CASE("rhi-deferred-destroy: a frame that submits nothing destroys nothing but is counted [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();
    constexpr int kBufferCount = 3;

    for (int index = 0; index < kBufferCount; ++index)
    {
        std::shared_ptr<RHIBuffer> buffer = MakeProbeBuffer(probe);
    }

    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == kBufferCount);

    // No batch reached the platform, so the criterion is not met: the queue holds the handles and
    // says so instead of destroying them on a boundary that never happened.
    for (int frame = 0; frame < 200; ++frame)
    {
        CHECK(rhi.RHINoteFrameWithoutSubmit() == kBufferCount);
    }
    CHECK(probe->Destroyed.load() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == kBufferCount);
    CHECK(rhi.GetDeferredDestroyQueue().GetSkippedSubmitCount() == 200);
    CHECK(rhi.GetDeferredDestroyQueue().GetCommittedBatchCount() == 0);

    // The escape hatch is the next real batch, not a frame count.
    CHECK(rhi.RHISubmitBatchCommitted() == kBufferCount);
    CHECK(probe->Destroyed.load() == kBufferCount);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 0);
}

TEST_CASE("rhi-deferred-destroy: a long stall warns once per interval, not once per frame [smoke]")
{
    HeadlessRHI rhi;
    RHIDeferredDestroyQueue& queue = rhi.GetDeferredDestroyQueue();
    queue.SetStallWarnIntervalMs(50);
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();

    {
        std::shared_ptr<RHIBuffer> buffer = MakeProbeBuffer(probe);
    }

    // Nothing warns while the queue is younger than the stall interval.
    CHECK(queue.GetStallWarnCount() == 0);
    rhi.RHINoteFrameWithoutSubmit();
    CHECK(queue.GetStallWarnCount() == 0);

    // Past the interval the missing submit is reported, and the report is limited: 200 skipped
    // frames inside one interval produce a single line.
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    for (int frame = 0; frame < 200; ++frame)
    {
        rhi.RHINoteFrameWithoutSubmit();
    }
    CHECK(queue.GetStallWarnCount() == 1);
    CHECK(queue.GetOldestPendingAgeMs() >= 60);
    CHECK(queue.GetPendingCount() == 1);
    CHECK(probe->Destroyed.load() == 0);
}

TEST_CASE("rhi-deferred-destroy: the stall warning is driven by the interval, not by a one-time flag [smoke]")
{
    HeadlessRHI rhi;
    RHIDeferredDestroyQueue& queue = rhi.GetDeferredDestroyQueue();
    // Zero means "how long a handle may wait before warning" is nothing: every evaluation is due.
    queue.SetStallWarnIntervalMs(0);
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();

    {
        std::shared_ptr<RHIBuffer> buffer = MakeProbeBuffer(probe);
    }

    // Posting into a queue that is already past its interval reports it too; every later
    // evaluation is due again because the interval is zero.
    const uint64_t warnedAtPost = queue.GetStallWarnCount();
    CHECK(warnedAtPost == 1);

    for (int frame = 0; frame < 5; ++frame)
    {
        rhi.RHINoteFrameWithoutSubmit();
    }
    CHECK(queue.GetStallWarnCount() == warnedAtPost + 5);
    CHECK(queue.GetPendingCount() == 1);
}

TEST_CASE("rhi-deferred-destroy: shutdown clears everything still queued without inventing a batch [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();
    constexpr int kBufferCount = 4;

    for (int index = 0; index < kBufferCount; ++index)
    {
        std::shared_ptr<RHIBuffer> buffer = MakeProbeBuffer(probe);
    }
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == kBufferCount);
    CHECK(probe->Destroyed.load() == 0);

    // Shutdown skips the criterion - the device is being torn down and cannot wait for a submit -
    // but it does not pretend a batch was committed.
    CHECK(rhi.RHIDrainAllDeferredDestroys() == kBufferCount);
    CHECK(probe->Destroyed.load() == kBufferCount);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetAbandonedCount() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetCommittedBatchCount() == 0);
}

TEST_CASE("rhi-deferred-destroy: a handle released with no RHI alive is reported, not destroyed elsewhere [smoke]")
{
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();
    std::shared_ptr<RHIBuffer> buffer;
    {
        HeadlessRHI rhi;
        buffer = MakeProbeBuffer(probe);
    }

    // The device dies before the last handle reference does, so no queue is left to run the
    // callback. Destroying it on the releasing thread would be undefined, so the layer reports the
    // leak instead of turning it into one.
    const uint64_t before = RHIDeferredDestroyQueue::GetPostedWithoutQueueCount();
    buffer.reset();
    const uint64_t after = RHIDeferredDestroyQueue::GetPostedWithoutQueueCount();

    CHECK(after == before + 1);
    CHECK(probe->Destroyed.load() == 0);
    CHECK(probe->DestroyAttempts.load() == 0);
}

TEST_CASE("rhi-deferred-destroy: the handle carries no queue, so the live device's queue takes it [smoke]")
{
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();
    std::shared_ptr<RHIBuffer> buffer;
    {
        HeadlessRHI first;
        buffer = MakeProbeBuffer(probe);
        CHECK(first.GetDeferredDestroyQueue().GetPendingCount() == 0);
    }

    HeadlessRHI second;
    buffer.reset();

    // Nothing was left behind on the dead device: the release found the queue of the device that is
    // alive now, because the resource does not remember the one that made it.
    CHECK(second.GetDeferredDestroyQueue().GetPendingCount() == 1);
    CHECK(probe->Destroyed.load() == 0);
    CHECK(second.RHISubmitBatchCommitted() == 1);
    CHECK(probe->Destroyed.load() == 1);
}

/**
 * One registry for the whole process, observed across the module boundary.
 *
 * The runtime module is a separate binary that includes the same header, so a header-local static
 * gives it a registry of its own. That is invisible from inside either module and looks exactly like
 * a lifetime bug: a host installs its queue into its own copy, and every handle a live device owns
 * that is released inside the runtime module is then counted as "dropped while no RHI was alive".
 * Installing through the entry point a device uses and reading the storage the runtime module
 * compiled is what makes the single instance observable.
 */
TEST_CASE("rhi-deferred-destroy: the active queue registry is one copy for the whole process [smoke]")
{
    const std::shared_ptr<RHIDeferredDestroyQueue> queue = std::make_shared<RHIDeferredDestroyQueue>();

    RHIDeferredDestroyQueue::InstallActiveQueue(queue);
    const std::shared_ptr<RHIDeferredDestroyQueue> observed =
        minEngine::RHIDeferredDestroyState::ActiveQueueSlot().lock();

    CHECK_MESSAGE(observed.get() == queue.get(), "the runtime module holds no queue after this one installed one");
    CHECK(RHIDeferredDestroyQueue::GetActiveQueue().get() == queue.get());

    RHIDeferredDestroyQueue::UninstallActiveQueue(queue.get());
    CHECK(minEngine::RHIDeferredDestroyState::ActiveQueueSlot().expired());
    CHECK(RHIDeferredDestroyQueue::GetActiveQueue() == nullptr);

    // The refusal counter is part of the same registry: the entry point this module calls has to
    // move the number the runtime module keeps, or a host that reads it always reads zero.
    const uint64_t before = minEngine::RHIDeferredDestroyState::PostedWithoutQueueCounter().load();
    CHECK_FALSE(RHIDeferredDestroyQueue::PostToActiveQueue([]() { return true; }, "module-state probe"));
    CHECK_MESSAGE(
        minEngine::RHIDeferredDestroyState::PostedWithoutQueueCounter().load() == before + 1,
        "the refusal was counted in a copy the runtime module does not hold");
    CHECK(RHIDeferredDestroyQueue::GetPostedWithoutQueueCount() == before + 1);
}

/**
 * The instance serial is a cache key, so two devices handed the same number would serve each other's
 * cached objects. A counter in the header would restart at one in every module; this asks the number
 * the runtime module keeps after this module constructed a device.
 */
TEST_CASE("rhi-deferred-destroy: an RHI takes its serial from the runtime module [smoke]")
{
    const uint64_t before = minEngine::RHIRegistry::CurrentInstanceSerial();

    HeadlessRHI rhi;

    CHECK_MESSAGE(
        minEngine::RHIRegistry::CurrentInstanceSerial() == rhi.GetInstanceSerial(),
        "this module handed out serial ", rhi.GetInstanceSerial(),
        " while the runtime module's counter stands at ", minEngine::RHIRegistry::CurrentInstanceSerial());
    CHECK(rhi.GetInstanceSerial() > before);
}

TEST_CASE("rhi-deferred-destroy: a release from inside a destroy callback waits for the next batch [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<DestroyProbe> outerProbe = std::make_shared<DestroyProbe>();
    const std::shared_ptr<DestroyProbe> innerProbe = std::make_shared<DestroyProbe>();

    std::shared_ptr<RHIBuffer> inner = MakeProbeBuffer(innerProbe);
    {
        // Dropping the outer handle inside the outer destroy callback stamps the inner one against
        // the batch being committed now, so it cannot ride the same tail.
        std::shared_ptr<RHIBuffer> outer =
            MakeProbeBuffer(outerProbe, [&inner]() { inner.reset(); });
    }

    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 1);
    CHECK(rhi.RHISubmitBatchCommitted() == 1);
    CHECK(outerProbe->Destroyed.load() == 1);
    CHECK(innerProbe->Destroyed.load() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 1);

    CHECK(rhi.RHISubmitBatchCommitted() == 1);
    CHECK(innerProbe->Destroyed.load() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 0);
}

TEST_CASE("rhi-deferred-destroy: the shutdown drain leaves nothing for the dying device to leak [smoke]")
{
    minEngine::LogSystem::Initialize();
    minEngine::LogConsoleStorage::Clear();

    std::shared_ptr<HeadlessRHI> rhi = std::make_shared<HeadlessRHI>();
    const std::shared_ptr<DestroyProbe> probe = std::make_shared<DestroyProbe>();

    constexpr int kHandles = 3;
    for (int index = 0; index < kHandles; ++index)
    {
        std::shared_ptr<RHIBuffer> buffer = MakeProbeBuffer(probe);
        CHECK(buffer != nullptr);
    }

    // Nothing reached a batch tail: the frame loop is over, so the queue is what the device would
    // have had to abandon.
    CHECK(rhi->GetDeferredDestroyQueue().GetPendingCount() == static_cast<size_t>(kHandles));

    // The shutdown drain takes them all, which is what makes the device's own teardown silent.
    CHECK(rhi->RHIDrainAllDeferredDestroys() == static_cast<size_t>(kHandles));
    CHECK(rhi->GetDeferredDestroyQueue().GetPendingCount() == 0);
    CHECK(rhi->GetDeferredDestroyQueue().GetAbandonedCount() == 0);
    CHECK(probe->Destroyed.load() == static_cast<uint32_t>(kHandles));

    rhi->Shutdown();
    rhi.reset();

    bool leaked = false;
    bool abandoned = false;
    for (const minEngine::LogRecord& record : minEngine::LogConsoleStorage::Snapshot())
    {
        if (record.severity != minEngine::LogSeverity::Error)
        {
            continue;
        }
        if (record.message.find("no RHI was alive") != std::string::npos)
        {
            leaked = true;
        }
        if (record.message.find("undestroyed") != std::string::npos)
        {
            abandoned = true;
        }
    }
    CHECK_FALSE(leaked);
    CHECK_FALSE(abandoned);
}

