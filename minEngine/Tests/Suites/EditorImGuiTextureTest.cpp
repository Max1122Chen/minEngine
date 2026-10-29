#include "EditorImGuiTextureTest.h"

#include "Platform/EditorRHIImGuiTexture.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHIRenderPass.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include "doctest.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using minEngine::FrameThreading;
using minEngine::FrameThreadKind;
using minEngine::FrameThreadMode;
using minEngine::ScopedFrameThreadKind;

using minEngine::EditorImGuiTextureBackendOps;
using minEngine::EditorRHIImGuiTexturePin;
using minEngine::EditorImGuiTextureRegistry;

namespace
{
    /**
     * Stands in for a backend RHI: the suites only need the queues the base hands out, so every
     * resource factory stays empty and the device-facing commands do nothing.
     */
    class HeadlessRHI final : public minEngine::RHI
    {
    public:
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

        void RHICmdBeginRenderPass(const minEngine::RHIRenderPassInfo&) override {}
        void RHICmdEndRenderPass() override {}
        void RHICmdSetGraphicsPipelineState(minEngine::RHIGraphicsPipelineState*) override {}
        void RHICmdSetShaderBindingSet(uint32_t, minEngine::RHIShaderBindingSet*) override {}
        void RHICmdTransition(const minEngine::RHITextureTransitionInfo&) override {}
        void RHICmdSetViewport(uint32_t, uint32_t, uint32_t, uint32_t, bool) override {}
        void RHICmdSetVertexBuffer(minEngine::RHIBuffer*, uint32_t) override {}
        void RHICmdSetIndexBuffer(minEngine::RHIBuffer*) override {}
        void RHICmdDrawIndexed(uint32_t, uint32_t, int32_t) override {}
        void RHICmdDraw(uint32_t, uint32_t) override {}
        void RHICmdGenerateMips(minEngine::RHITexture*) override {}
        void RHISetBackbufferClearColor(const minEngine::Math::Vector3&) override {}
        void RHIClearBackbuffer() override {}
        void RHIPresent() override {}
    };

    /** A texture whose native half is a number, so a pin can name it without a driver. */
    class ProbeTexture final : public minEngine::RHITexture
    {
    public:
        ProbeTexture() = default;

        const minEngine::RHITextureCreateDesc& GetDesc() const override { return m_Desc; }
        void* GetNativeResource() const override
        {
            return reinterpret_cast<void*>(static_cast<uintptr_t>(m_Name));
        }
        uint32_t GetNativeHandle() const override { return m_Name; }

    private:
        minEngine::RHITextureCreateDesc m_Desc{};
        uint32_t m_Name = 7;
    };

    /**
     * What the backend calls of one test case did, and where. One probe per case keeps the
     * assertions about "which thread did the device work" independent of what other cases ran.
     */
    struct GpuProbe
    {
        std::mutex TraceMutex;
        std::vector<std::string> Trace;

        std::atomic<int> Creates{ 0 };
        std::atomic<int> Releases{ 0 };
        std::atomic<int> Rebuilds{ 0 };

        std::atomic<std::thread::id> CreateThread{};
        std::atomic<std::thread::id> ReleaseThread{};
        std::atomic<std::thread::id> RebuildThread{};
        /** Whether the rebuild callback still saw the request flag set when it ran. */
        std::atomic<bool> bRebuildSawPendingRequest{ false };

        void Record(const char* event)
        {
            std::lock_guard<std::mutex> lock(TraceMutex);
            Trace.emplace_back(event);
        }

        std::size_t CountEvents(const char* event)
        {
            std::lock_guard<std::mutex> lock(TraceMutex);
            std::size_t count = 0;
            for (const std::string& entry : Trace)
            {
                if (entry == event)
                {
                    ++count;
                }
            }
            return count;
        }
    };

    /** The three device calls, shaped exactly like the ones the renderer backend installs. */
    EditorImGuiTextureBackendOps MakeOps(const std::shared_ptr<GpuProbe>& probe)
    {
        EditorImGuiTextureBackendOps ops;
        ops.CreateIdentifier = [probe](minEngine::RHI*, minEngine::RHITexture*) -> std::uint64_t {
            const int creates = probe->Creates.fetch_add(1) + 1;
            probe->CreateThread.store(std::this_thread::get_id());
            probe->Record("create");
            return 0x1000u + static_cast<std::uint64_t>(creates);
        };
        ops.ReleaseIdentifier = [probe](minEngine::RHI*, std::uint64_t) {
            probe->Releases.fetch_add(1);
            probe->ReleaseThread.store(std::this_thread::get_id());
            probe->Record("release");
        };
        ops.RebuildDeviceObjects = [probe](minEngine::RHI*) {
            probe->bRebuildSawPendingRequest.store(EditorImGuiTextureRegistry::IsFontAtlasRebuildPending());
            probe->Rebuilds.fetch_add(1);
            probe->RebuildThread.store(std::this_thread::get_id());
            probe->Record("rebuild");
        };
        return ops;
    }

    /**
     * FrameThreading is a process-wide singleton: acquisition always owns the release, so a case
     * that fails halfway cannot leave the next one comparing against a destroyed object.
     */
    struct FrameThreadingScope
    {
        FrameThreading Threading;

        explicit FrameThreadingScope(FrameThreadMode mode) { Threading.Initialize(mode); }
        ~FrameThreadingScope() { Threading.Shutdown(); }

        FrameThreadingScope(const FrameThreadingScope&) = delete;
        FrameThreadingScope& operator=(const FrameThreadingScope&) = delete;
    };

    /**
     * The batch tail is an RHI-role duty, so a case that holds that role already runs it directly
     * and every other one claims the role for the call. Returns how many identifiers died.
     */
    std::size_t DrainBatchTail(minEngine::RHI& rhi)
    {
        if (FrameThreading::HasInstance() && !FrameThreading::Get().IsInRHIThread())
        {
            ScopedFrameThreadKind rhiRole(FrameThreadKind::RHI);
            return rhi.RHISubmitBatchCommitted();
        }
        return rhi.RHISubmitBatchCommitted();
    }
}

TEST_CASE("editor-imgui-texture: registering a texture drives the device on the graphics role, not on the UI thread [smoke]")
{
    FrameThreadingScope threading(FrameThreadMode::Multi);
    HeadlessRHI rhi;
    const std::shared_ptr<GpuProbe> probe = std::make_shared<GpuProbe>();
    EditorImGuiTextureRegistry::InstallBackendOps(MakeOps(probe));

    ProbeTexture texture;
    EditorRHIImGuiTexturePin pin;

    // The UI thread asks while it builds the frame. It must not touch the device: the registration
    // is a request for the graphics role.
    CHECK(pin.Pin(&rhi, &texture) == ImTextureID_Invalid);
    CHECK(probe->Creates.load() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 1);

    // The graphics role drains the request, and that is where the identifier is built.
    std::thread::id graphicsThreadId;
    std::thread graphicsThread([&rhi, &graphicsThreadId]() {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        graphicsThreadId = std::this_thread::get_id();
        rhi.RHIDrainDeferredCreates();
    });
    graphicsThread.join();

    CHECK(graphicsThreadId != std::this_thread::get_id());
    CHECK(probe->Creates.load() == 1);
    CHECK(probe->CreateThread.load() == graphicsThreadId);

    // From now on the UI thread gets the identifier without doing any device work itself.
    const ImTextureID identifier = pin.Pin(&rhi, &texture);
    CHECK(identifier != ImTextureID_Invalid);
    CHECK(probe->Creates.load() == 1);
    CHECK(EditorImGuiTextureRegistry::GetLiveBindingCount() == 1);

    pin.Reset(&rhi);
}

TEST_CASE("editor-imgui-texture: an unregistered identifier survives the call and dies at the batch tail [smoke]")
{
    // No frame roles: this thread is the graphics role, so the identifier exists immediately and
    // only the release timing is under test.
    HeadlessRHI rhi;
    const std::shared_ptr<GpuProbe> probe = std::make_shared<GpuProbe>();
    EditorImGuiTextureRegistry::InstallBackendOps(MakeOps(probe));

    ProbeTexture texture;
    EditorRHIImGuiTexturePin pin;

    const ImTextureID identifier = pin.Pin(&rhi, &texture);
    REQUIRE(identifier != ImTextureID_Invalid);
    CHECK(probe->Creates.load() == 1);
    CHECK(probe->Releases.load() == 0);

    const EditorImGuiTextureRegistry::Counters before = EditorImGuiTextureRegistry::GetCounters();
    pin.Reset(&rhi);

    // The identifier is retired, not freed: the batch that could still name it is not with the
    // platform yet, which is exactly the boundary the destroy queue waits for.
    CHECK(probe->Releases.load() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 1);
    const EditorImGuiTextureRegistry::Counters afterRelease = EditorImGuiTextureRegistry::GetCounters();
    CHECK(afterRelease.ReleasesRetired == before.ReleasesRetired + 1);
    CHECK(afterRelease.ReleasesDropped == before.ReleasesDropped);

    CHECK(DrainBatchTail(rhi) == 1);
    CHECK(probe->Releases.load() == 1);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 0);
    CHECK(EditorImGuiTextureRegistry::GetLiveBindingCount() == 0);
}

TEST_CASE("editor-imgui-texture: a graphics-role entry reached from the game thread is recorded and harmless [smoke]")
{
    // Multi topology with only the game role bound: this thread may not drive the device.
    FrameThreadingScope threading(FrameThreadMode::Multi);
    HeadlessRHI rhi;
    const std::shared_ptr<GpuProbe> probe = std::make_shared<GpuProbe>();
    EditorImGuiTextureRegistry::InstallBackendOps(MakeOps(probe));

    EditorImGuiTextureRegistry::RequestFontAtlasRebuild();
    const EditorImGuiTextureRegistry::Counters before = EditorImGuiTextureRegistry::GetCounters();
    const minEngine::ContractViolationCounters violationsBefore =
        minEngine::GetContractViolationCounters();

    EditorImGuiTextureRegistry::PerformPendingGpuWork(&rhi);

    const EditorImGuiTextureRegistry::Counters after = EditorImGuiTextureRegistry::GetCounters();
    const minEngine::ContractViolationCounters violationsAfter = minEngine::GetContractViolationCounters();

    // Counted here and counted by the contract report: a silently ignored call would look like
    // "the rebuild happened" instead of "the rebuild went to the wrong thread".
    CHECK(after.OffRoleRefusals == before.OffRoleRefusals + 1);
    CHECK(violationsAfter.Total() == violationsBefore.Total() + 1);
    CHECK(probe->Rebuilds.load() == 0);
    // The request was not consumed by the refused call.
    CHECK(EditorImGuiTextureRegistry::IsFontAtlasRebuildPending());

    // And the graphics role then performs exactly what the UI thread asked for.
    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        EditorImGuiTextureRegistry::PerformPendingGpuWork(&rhi);
    }
    CHECK(probe->Rebuilds.load() == 1);
    CHECK_FALSE(EditorImGuiTextureRegistry::IsFontAtlasRebuildPending());
}

TEST_CASE("editor-imgui-texture: a font atlas rebuild is a request the graphics role performs in one step [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<GpuProbe> probe = std::make_shared<GpuProbe>();
    EditorImGuiTextureRegistry::InstallBackendOps(MakeOps(probe));

    ProbeTexture texture;
    EditorRHIImGuiTexturePin pin;
    REQUIRE(pin.Pin(&rhi, &texture) != ImTextureID_Invalid);
    CHECK(probe->Creates.load() == 1);

    const EditorImGuiTextureRegistry::Counters before = EditorImGuiTextureRegistry::GetCounters();
    EditorImGuiTextureRegistry::RequestFontAtlasRebuild();
    CHECK(EditorImGuiTextureRegistry::IsFontAtlasRebuildPending());
    CHECK(probe->Rebuilds.load() == 0);
    CHECK(probe->CountEvents("rebuild") == 0);

    EditorImGuiTextureRegistry::PerformPendingGpuWork(&rhi);

    // Destroy and recreate in one step, and the flag is already cleared when the device work runs:
    // a second request can be posted from the callback without being lost.
    CHECK(probe->Rebuilds.load() == 1);
    CHECK(probe->CountEvents("rebuild") == 1);
    CHECK_FALSE(probe->bRebuildSawPendingRequest.load());
    CHECK_FALSE(EditorImGuiTextureRegistry::IsFontAtlasRebuildPending());

    const EditorImGuiTextureRegistry::Counters after = EditorImGuiTextureRegistry::GetCounters();
    CHECK(after.FontAtlasRebuildRequests == before.FontAtlasRebuildRequests + 1);
    CHECK(after.FontAtlasRebuilds == before.FontAtlasRebuilds + 1);
    // The descriptor pool the identifier lived in went with the device objects.
    CHECK(after.IdentifiersDiscarded == before.IdentifiersDiscarded + 1);

    // The pin re-registers instead of drawing with an identifier whose pool no longer exists.
    const EditorImGuiTextureRegistry::Counters beforeRepin = EditorImGuiTextureRegistry::GetCounters();
    CHECK(pin.Pin(&rhi, &texture) != ImTextureID_Invalid);
    CHECK(probe->Creates.load() == 2);
    CHECK(EditorImGuiTextureRegistry::GetCounters().Registered == beforeRepin.Registered + 1);
}

TEST_CASE("editor-imgui-texture: invalidating bindings retires them instead of freeing them in the UI call [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<GpuProbe> probe = std::make_shared<GpuProbe>();
    EditorImGuiTextureRegistry::InstallBackendOps(MakeOps(probe));

    ProbeTexture firstTexture;
    ProbeTexture secondTexture;
    EditorRHIImGuiTexturePin firstPin;
    EditorRHIImGuiTexturePin secondPin;
    REQUIRE(firstPin.Pin(&rhi, &firstTexture) != ImTextureID_Invalid);
    REQUIRE(secondPin.Pin(&rhi, &secondTexture) != ImTextureID_Invalid);
    CHECK(probe->Creates.load() == 2);
    CHECK(EditorImGuiTextureRegistry::GetLiveBindingCount() == 2);

    EditorRHIImGuiTexturePin::InvalidateAllBindings();

    // Nothing was freed where the UI code asked; both are queued for the next batch tail.
    CHECK(probe->Releases.load() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 2);
    CHECK(EditorImGuiTextureRegistry::GetLiveBindingCount() == 0);

    CHECK(DrainBatchTail(rhi) == 2);
    CHECK(probe->Releases.load() == 2);
}

TEST_CASE("editor-imgui-texture: a destroyed pin leaves nothing behind for another thread to walk [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<GpuProbe> probe = std::make_shared<GpuProbe>();
    EditorImGuiTextureRegistry::InstallBackendOps(MakeOps(probe));

    ProbeTexture texture;
    {
        EditorRHIImGuiTexturePin pin;
        REQUIRE(pin.Pin(&rhi, &texture) != ImTextureID_Invalid);
        CHECK(EditorImGuiTextureRegistry::GetLiveBindingCount() == 1);
    }

    // The binding list is the only thing the graphics role and the registry walk, and it holds weak
    // references: the pin going away is the whole cleanup.
    CHECK(EditorImGuiTextureRegistry::GetLiveBindingCount() == 0);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 1);

    CHECK(DrainBatchTail(rhi) == 1);
    CHECK(probe->Releases.load() == 1);
}

TEST_CASE("editor-imgui-texture: a release queued before the backend went away cannot call a dead device [smoke]")
{
    HeadlessRHI rhi;
    const std::shared_ptr<GpuProbe> probe = std::make_shared<GpuProbe>();
    EditorImGuiTextureRegistry::InstallBackendOps(MakeOps(probe));

    ProbeTexture texture;
    EditorRHIImGuiTexturePin pin;
    REQUIRE(pin.Pin(&rhi, &texture) != ImTextureID_Invalid);
    pin.Reset(&rhi);
    CHECK(rhi.GetDeferredDestroyQueue().GetPendingCount() == 1);

    const EditorImGuiTextureRegistry::Counters before = EditorImGuiTextureRegistry::GetCounters();
    EditorImGuiTextureRegistry::UninstallBackendOps();

    // The shutdown drain runs the queued callbacks after the backend is gone; the identifier went
    // with the pool, so the callback has to be a no-op rather than a call into freed state.
    CHECK(DrainBatchTail(rhi) == 1);
    CHECK(probe->Releases.load() == 0);
    CHECK(EditorImGuiTextureRegistry::GetCounters().ReleasesDropped == before.ReleasesDropped + 1);
}
