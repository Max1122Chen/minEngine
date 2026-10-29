#include "RHIDeferredCreateTest.h"

#include "Runtime/Core/Log/LogRecord.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIRenderPass.h"
#include "Runtime/Function/Render/RHI/RHISampler.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"

#include "doctest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using minEngine::FrameThreadKind;
using minEngine::FrameThreadMode;
using minEngine::FrameThreading;
using minEngine::RHIBuffer;
using minEngine::RHIBufferCreateDesc;
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
using minEngine::ScopedFrameThreadKind;
using minEngine::TaskSystem;

namespace
{
    /**
     * What the resources of one case record about their native creation: how many ran, on which
     * thread, and in what order. Order is the property that has to survive the queue, so it is
     * recorded across resources rather than per resource.
     */
    class NativeCreateLog
    {
    public:
        void RecordCreate(int resourceId, std::thread::id thread)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Sequence.push_back(resourceId);
            ++m_CreateAttempts;
            m_ThreadOfCreate = thread;
        }

        void RecordUpdate(bool bNativeObjectExisted)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (bNativeObjectExisted)
            {
                ++m_Updates;
            }
            else
            {
                ++m_UpdatesBeforeCreate;
            }
        }

        int GetCreateAttempts() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_CreateAttempts;
        }

        std::vector<int> GetSequence() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Sequence;
        }

        std::thread::id GetThreadOfCreate() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_ThreadOfCreate;
        }

        int GetUpdates() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Updates;
        }

        int GetUpdatesBeforeCreate() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_UpdatesBeforeCreate;
        }

    private:
        mutable std::mutex m_Mutex;
        std::vector<int> m_Sequence;
        int m_CreateAttempts = 0;
        std::thread::id m_ThreadOfCreate{};
        int m_Updates = 0;
        int m_UpdatesBeforeCreate = 0;
    };

    /**
     * A resource whose native half is a log entry. It takes the shape a backend takes: the handle
     * exists as soon as the create call returns, the native object only once CreateNativeResources
     * has run, and until then the handle answers "not ready".
     */
    class ProbeBuffer final : public RHIBuffer
    {
    public:
        ProbeBuffer(
            const RHIBufferCreateDesc& desc,
            std::shared_ptr<NativeCreateLog> log,
            int resourceId)
            : m_Desc(desc)
            , m_Log(std::move(log))
            , m_ResourceId(resourceId)
        {
        }

        ProbeBuffer(const ProbeBuffer&) = delete;
        ProbeBuffer& operator=(const ProbeBuffer&) = delete;

        const RHIBufferCreateDesc& GetDesc() const override { return m_Desc; }

        void UpdateSubresource(const void*, uint32_t, uint32_t) override
        {
            // Same contract the OpenGL buffer keeps: a write that lands before the native object
            // exists has nowhere to go and is refused instead of being sent to whatever object is
            // bound.
            m_Log->RecordUpdate(m_bHasNativeObject);
        }

        bool CreateNativeResources() override
        {
            if (m_bHasNativeObject)
            {
                return true;
            }
            if (!m_bCanCreateNow.load(std::memory_order_relaxed))
            {
                return false;
            }

            m_bHasNativeObject = true;
            m_Log->RecordCreate(m_ResourceId, std::this_thread::get_id());
            return true;
        }

        bool IsReady() const override { return m_bHasNativeObject; }

        /** Stands in for "this frame start cannot create it yet". */
        void SetCanCreateNow(bool bCanCreate)
        {
            m_bCanCreateNow.store(bCanCreate, std::memory_order_relaxed);
        }

    private:
        RHIBufferCreateDesc m_Desc;
        std::shared_ptr<NativeCreateLog> m_Log;
        int m_ResourceId = 0;
        bool m_bHasNativeObject = false;
        std::atomic<bool> m_bCanCreateNow{ true };
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

    /**
     * What every probe resource records about its native creation: which kind ran, on which
     * thread, and which one refused. One log across resources keeps the order visible the way the
     * buffer log above does.
     */
    class ProbeNativeLog
    {
    public:
        void RecordCreate(const char* kind, std::thread::id thread)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_CreatedKinds.emplace_back(kind);
            m_ThreadOfCreate = thread;
        }

        void RecordRefusal(const char* kind)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_RefusedKinds.emplace_back(kind);
        }

        std::vector<std::string> GetCreatedKinds() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_CreatedKinds;
        }

        std::vector<std::string> GetRefusedKinds() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_RefusedKinds;
        }

        size_t GetCreateCount() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_CreatedKinds.size();
        }

        std::thread::id GetThreadOfCreate() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_ThreadOfCreate;
        }

    private:
        mutable std::mutex m_Mutex;
        std::vector<std::string> m_CreatedKinds;
        std::vector<std::string> m_RefusedKinds;
        std::thread::id m_ThreadOfCreate{};
    };

    /**
     * The native half the probe programs and binding objects share: nothing happens where the
     * handle is built, one attempt happens where the graphics role runs it, and a resource told to
     * fail refuses for good instead of asking the driver again every frame start.
     */
    class ProbeNativeState
    {
    public:
        bool Create(const std::shared_ptr<ProbeNativeLog>& log, const char* kind)
        {
            if (m_bCreated)
            {
                return true;
            }
            if (m_bShouldFail)
            {
                m_bFailed = true;
                log->RecordRefusal(kind);
                return false;
            }

            m_bCreated = true;
            log->RecordCreate(kind, std::this_thread::get_id());
            return true;
        }

        bool IsReady() const { return m_bCreated; }
        bool HasFailed() const { return m_bFailed; }
        void SetShouldFail(bool bShouldFail) { m_bShouldFail = bShouldFail; }

    private:
        bool m_bCreated = false;
        bool m_bShouldFail = false;
        bool m_bFailed = false;
    };

    /** A program handle: created where the graphics role runs it, invalid until then. */
    class ProbeShader final : public RHIShader
    {
    public:
        explicit ProbeShader(std::shared_ptr<ProbeNativeLog> log)
            : m_Log(std::move(log))
        {
        }

        bool IsValid() const override { return m_Native.IsReady(); }
        const std::string& GetCompileLog() const override { return m_CompileLog; }
        bool CreateNativeResources() override
        {
            const bool bCreated = m_Native.Create(m_Log, "probe shader program");
            if (!bCreated && m_CompileLog.empty())
            {
                // Stands in for the driver's message: the one report this failure gets.
                m_CompileLog = "probe shader: the driver refused the program";
                ME_LOG(minEngine::LogRHI, Error, "{}", m_CompileLog);
            }
            return bCreated;
        }
        bool IsReady() const override { return m_Native.IsReady(); }
        bool HasNativeCreateFailed() const override { return m_Native.HasFailed(); }

        void SetShouldFail(bool bShouldFail) { m_Native.SetShouldFail(bShouldFail); }

    private:
        std::shared_ptr<ProbeNativeLog> m_Log;
        ProbeNativeState m_Native;
        std::string m_CompileLog;
    };

    class ProbePipelineState final : public RHIGraphicsPipelineState
    {
    public:
        explicit ProbePipelineState(std::shared_ptr<ProbeNativeLog> log, bool bOwnsNativeObject)
            : m_Log(std::move(log))
            , m_bOwnsNativeObject(bOwnsNativeObject)
        {
        }

        bool CreateNativeResources() override
        {
            return !m_bOwnsNativeObject || m_Native.Create(m_Log, "probe pipeline state");
        }
        bool IsReady() const override { return !m_bOwnsNativeObject || m_Native.IsReady(); }

    private:
        std::shared_ptr<ProbeNativeLog> m_Log;
        bool m_bOwnsNativeObject = true;
        ProbeNativeState m_Native;
    };

    class ProbeBindingSetLayout final : public RHIShaderBindingSetLayout
    {
    public:
        explicit ProbeBindingSetLayout(std::shared_ptr<ProbeNativeLog> log)
            : m_Log(std::move(log))
        {
        }

        const std::vector<RHIShaderBindingSetLayoutEntry>& GetEntries() const override
        {
            return m_Entries;
        }
        bool CreateNativeResources() override
        {
            return m_Native.Create(m_Log, "probe binding set layout");
        }
        bool IsReady() const override { return m_Native.IsReady(); }

    private:
        std::shared_ptr<ProbeNativeLog> m_Log;
        std::vector<RHIShaderBindingSetLayoutEntry> m_Entries;
        ProbeNativeState m_Native;
    };

    class ProbeBindingSet final : public RHIShaderBindingSet
    {
    public:
        explicit ProbeBindingSet(std::shared_ptr<ProbeNativeLog> log)
            : m_Log(std::move(log))
        {
        }

        const RHIShaderBindingSetLayout* GetLayout() const override { return m_Layout; }
        const std::vector<RHIShaderBinding>& GetBindings() const override { return m_Bindings; }
        bool CreateNativeResources() override
        {
            return m_Native.Create(m_Log, "probe binding set");
        }
        bool IsReady() const override { return m_Native.IsReady(); }

    private:
        std::shared_ptr<ProbeNativeLog> m_Log;
        RHIShaderBindingSetLayout* m_Layout = nullptr;
        std::vector<RHIShaderBinding> m_Bindings;
        ProbeNativeState m_Native;
    };

    class ProbeSampler final : public RHISampler
    {
    public:
        ProbeSampler(std::shared_ptr<ProbeNativeLog> log, const RHISamplerDesc& desc)
            : m_Log(std::move(log))
            , m_Desc(desc)
        {
        }

        const RHISamplerDesc& GetDesc() const override { return m_Desc; }
        bool CreateNativeResources() override { return m_Native.Create(m_Log, "probe sampler"); }
        bool IsReady() const override { return m_Native.IsReady(); }

    private:
        std::shared_ptr<ProbeNativeLog> m_Log;
        RHISamplerDesc m_Desc;
        ProbeNativeState m_Native;
    };

    /**
     * Records every log record that matches one substring while it is armed. Used to count the
     * reports a failed creation produces: exactly one is the property under test.
     */
    class MatchingLogSink final : public minEngine::ILogSink
    {
    public:
        void Write(const minEngine::LogRecord& record) override
        {
            if (!m_bArmed.load(std::memory_order_acquire))
            {
                return;
            }
            if (record.severity != minEngine::LogSeverity::Error)
            {
                return;
            }
            if (record.message.find(m_Substring) == std::string::npos)
            {
                return;
            }
            m_Matches.fetch_add(1, std::memory_order_relaxed);
        }

        void Arm(const std::string& substring)
        {
            m_Substring = substring;
            m_Matches.store(0, std::memory_order_relaxed);
            m_bArmed.store(true, std::memory_order_release);
        }

        uint32_t Disarm()
        {
            m_bArmed.store(false, std::memory_order_release);
            return m_Matches.load(std::memory_order_relaxed);
        }

    private:
        std::atomic<bool> m_bArmed{ false };
        std::atomic<uint32_t> m_Matches{ 0 };
        std::string m_Substring;
    };

    MatchingLogSink& GetSharedLogSink()
    {
        static MatchingLogSink s_Sink;
        return s_Sink;
    }

    /** Registers the shared sink once per process; the log system owns the reference. */
    void EnsureLogSinkInstalled()
    {
        static std::once_flag s_Once;
        std::call_once(s_Once, []() {
            minEngine::LogSystem::AddSink(std::shared_ptr<minEngine::ILogSink>(
                &GetSharedLogSink(),
                [](minEngine::ILogSink*) {
                    // Process-lifetime probe: the log system keeps a reference to it, so it must
                    // outlive every sink call - including the ones after static destruction.
                }));
        });
    }

    /**
     * Stands in for a backend RHI. Its buffer factory is the real policy - hand the handle to
     * ScheduleNativeCreate and let the base decide whether the native object is created now or at
     * the next frame start - so these cases exercise the shared path rather than a copy of it.
     */
    class HeadlessRHI final : public minEngine::RHI
    {
    public:
        explicit HeadlessRHI(std::shared_ptr<NativeCreateLog> log)
            : m_Log(std::move(log))
            , m_ProgramLog(std::make_shared<ProbeNativeLog>())
        {
        }

        const std::shared_ptr<ProbeNativeLog>& GetProgramLog() const { return m_ProgramLog; }

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
        std::shared_ptr<RHIBuffer> RHICreateBuffer(const RHIBufferCreateDesc& desc, const void*) override
        {
            auto buffer = std::make_shared<ProbeBuffer>(desc, m_Log, m_NextResourceId++);
            return ScheduleNativeCreate(std::move(buffer), "probe buffer");
        }
        /**
         * The program factories follow the same policy: the handle exists at once and the driver
         * object only once the graphics role runs it, which is the shape a shader has to take for
         * a material compiled on the game thread.
         */
        std::shared_ptr<RHIShader> RHICreateShader(
            const RHIShaderCreateDesc&,
            std::string* outCompileLog) override
        {
            auto shader = std::make_shared<ProbeShader>(m_ProgramLog);
            if (outCompileLog != nullptr)
            {
                *outCompileLog = shader->GetCompileLog();
            }
            m_LastShader = std::move(shader);
            return ScheduleNativeCreate(m_LastShader, "probe shader program");
        }
        std::shared_ptr<RHIShader> RHICreateShader(
            const std::string&,
            const std::string&,
            std::string* outCompileLog) override
        {
            auto shader = std::make_shared<ProbeShader>(m_ProgramLog);
            if (outCompileLog != nullptr)
            {
                *outCompileLog = shader->GetCompileLog();
            }
            m_LastShader = std::move(shader);
            return ScheduleNativeCreate(m_LastShader, "probe shader program");
        }
        std::shared_ptr<RHIGraphicsPipelineState> RHICreateGraphicsPipelineState(
            const RHIGraphicsPSODesc&) override
        {
            // A pipeline state that owns no driver object is ready where it is built and never
            // reaches the queue; one that does is created by the graphics role.
            return ScheduleNativeCreate(
                std::make_shared<ProbePipelineState>(m_ProgramLog, m_bPipelineStateOwnsNativeObject),
                "probe pipeline state");
        }
        std::shared_ptr<RHIShaderBindingSetLayout> RHICreateShaderBindingSetLayout(
            const std::vector<RHIShaderBindingSetLayoutEntry>&) override
        {
            return ScheduleNativeCreate(
                std::make_shared<ProbeBindingSetLayout>(m_ProgramLog),
                "probe binding set layout");
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
            return ScheduleNativeCreate(
                std::make_shared<ProbeBindingSet>(m_ProgramLog),
                "probe binding set");
        }
        std::shared_ptr<RHISampler> RHIGetOrCreateSampler(const RHISamplerDesc& desc) override
        {
            return ScheduleNativeCreate(
                std::make_shared<ProbeSampler>(m_ProgramLog, desc),
                "probe sampler");
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

        std::shared_ptr<ProbeBuffer> CreateProbeBuffer()
        {
            return std::dynamic_pointer_cast<ProbeBuffer>(RHICreateBuffer(MakeProbeBufferDesc(), nullptr));
        }

        std::shared_ptr<ProbeShader> CreateProbeShader()
        {
            return std::dynamic_pointer_cast<ProbeShader>(RHICreateShader(RHIShaderCreateDesc{}, nullptr));
        }

        /** Whether this backend's pipeline state owns a driver object; the probe models both. */
        void SetPipelineStateOwnsNativeObject(bool bOwns) { m_bPipelineStateOwnsNativeObject = bOwns; }

    private:
        std::shared_ptr<NativeCreateLog> m_Log;
        std::shared_ptr<ProbeNativeLog> m_ProgramLog;
        std::shared_ptr<RHIShader> m_LastShader;
        bool m_bPipelineStateOwnsNativeObject = false;
        int m_NextResourceId = 0;
    };
}

TEST_CASE("rhi-deferred-create: a create off the graphics role queues the native object [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    const std::shared_ptr<RHIBuffer> buffer = rhi.RHICreateBuffer(MakeProbeBufferDesc(), nullptr);

    // The handle is complete and usable as a value; the driver object is not there yet, and the
    // create call did not touch it.
    CHECK(buffer != nullptr);
    CHECK(!buffer->IsReady());
    CHECK(log->GetCreateAttempts() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 1);
    CHECK(rhi.GetDeferredCreateQueue().GetPostedCount() == 1);
    CHECK(rhi.GetDeferredCreateQueue().GetCreatedCount() == 0);

    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: the frame-start drain creates it on the graphics role thread [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    const std::shared_ptr<RHIBuffer> buffer = rhi.RHICreateBuffer(MakeProbeBufferDesc(), nullptr);
    CHECK(!buffer->IsReady());

    std::atomic<bool> graphicsRoleHeld{ false };
    std::atomic<std::thread::id> graphicsThreadId{};
    tasks.StartFrameRoles(
        [&rhi, &graphicsRoleHeld, &graphicsThreadId](float) {
            graphicsRoleHeld.store(
                FrameThreading::Get().IsInRenderingThread(),
                std::memory_order_relaxed);
            graphicsThreadId.store(std::this_thread::get_id(), std::memory_order_release);
            rhi.RHIDrainDeferredCreates();
        },
        []() {},
        /*combineGraphicsThreads=*/true);

    tasks.SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);

    CHECK(graphicsRoleHeld.load());
    CHECK(buffer->IsReady());
    CHECK(log->GetCreateAttempts() == 1);
    CHECK(log->GetThreadOfCreate() == graphicsThreadId.load());
    CHECK(log->GetThreadOfCreate() != std::this_thread::get_id());
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetCreatedCount() == 1);

    tasks.Shutdown();
    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: a create on the graphics role creates natively and queues nothing [smoke]")
{
    // Single topology binds every role to one thread, so the calling thread already is the
    // graphics role and nothing may be deferred.
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Single);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    const std::shared_ptr<RHIBuffer> buffer = rhi.RHICreateBuffer(MakeProbeBufferDesc(), nullptr);

    CHECK(buffer != nullptr);
    CHECK(buffer->IsReady());
    CHECK(log->GetCreateAttempts() == 1);
    CHECK(log->GetThreadOfCreate() == std::this_thread::get_id());
    CHECK(rhi.GetDeferredCreateQueue().GetPostedCount() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 0);

    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: resources materialize in the order they were created [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    constexpr int kBufferCount = 5;
    std::vector<std::shared_ptr<RHIBuffer>> buffers;
    buffers.reserve(kBufferCount);
    for (int index = 0; index < kBufferCount; ++index)
    {
        buffers.push_back(rhi.RHICreateBuffer(MakeProbeBufferDesc(), nullptr));
    }
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == kBufferCount);
    CHECK(log->GetCreateAttempts() == 0);

    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        CHECK(rhi.RHIDrainDeferredCreates() == kBufferCount);
    }

    const std::vector<int> expectedOrder{ 0, 1, 2, 3, 4 };
    CHECK(log->GetSequence() == expectedOrder);
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 0);

    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: a drain off the graphics role is refused and stays queued [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    const std::shared_ptr<RHIBuffer> buffer = rhi.RHICreateBuffer(MakeProbeBufferDesc(), nullptr);

    // The game role holds no graphics context, so running the creation here would leave the
    // native object at zero again - the exact state the queue exists to avoid.
    CHECK(rhi.RHIDrainDeferredCreates() == 0);
    CHECK(!buffer->IsReady());
    CHECK(log->GetCreateAttempts() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 1);
    CHECK(rhi.GetDeferredCreateQueue().GetRefusedDrainCount() == 1);

    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: queued requests that never reached the graphics role are dropped at shutdown [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();

    std::shared_ptr<RHIBuffer> surviving;
    {
        HeadlessRHI rhi(log);
        surviving = rhi.RHICreateBuffer(MakeProbeBufferDesc(), nullptr);
        CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 1);

        // Explicit shutdown of the queue: reported and counted, never run.
        CHECK(rhi.GetDeferredCreateQueue().AbandonPending("test shutdown") == 1);
        CHECK(rhi.GetDeferredCreateQueue().GetAbandonedCount() == 1);
        CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 0);
        CHECK(rhi.RHIDrainDeferredCreates() == 0);
    }

    CHECK(log->GetCreateAttempts() == 0);
    CHECK(!surviving->IsReady());

    // The RHI's own destructor takes the same path, so nothing materializes on a device that is
    // already gone.
    {
        HeadlessRHI rhi(log);
        const std::shared_ptr<RHIBuffer> alsoDropped = rhi.RHICreateBuffer(MakeProbeBufferDesc(), nullptr);
        CHECK(!alsoDropped->IsReady());
    }
    CHECK(log->GetCreateAttempts() == 0);

    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: a consumer sees a handle without a native object and its write is refused [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    const std::shared_ptr<RHIBuffer> buffer = rhi.RHICreateBuffer(MakeProbeBufferDesc(), nullptr);

    // The expression a consumer can check before it draws or writes.
    REQUIRE(!buffer->IsReady());

    const uint32_t payload = 0;
    buffer->UpdateSubresource(&payload, 0, sizeof(payload));
    CHECK(log->GetUpdatesBeforeCreate() == 1);
    CHECK(log->GetUpdates() == 0);

    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        CHECK(rhi.RHIDrainDeferredCreates() == 1);
    }

    REQUIRE(buffer->IsReady());
    buffer->UpdateSubresource(&payload, 0, sizeof(payload));
    CHECK(log->GetUpdates() == 1);
    CHECK(log->GetUpdatesBeforeCreate() == 1);

    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: a handle nobody holds any more is not materialized [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    {
        const std::shared_ptr<RHIBuffer> dropped = rhi.RHICreateBuffer(MakeProbeBufferDesc(), nullptr);
        CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 1);
    }

    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        CHECK(rhi.RHIDrainDeferredCreates() == 0);
    }

    // Creating a native object for a handle nobody references would only queue it for
    // destruction at the next present boundary.
    CHECK(log->GetCreateAttempts() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetNotNeededCount() == 1);

    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: a create that fails at the frame start keeps its place for the next one [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    const std::shared_ptr<ProbeBuffer> blocked = rhi.CreateProbeBuffer();
    const std::shared_ptr<ProbeBuffer> ready = rhi.CreateProbeBuffer();
    REQUIRE(blocked != nullptr);
    REQUIRE(ready != nullptr);
    blocked->SetCanCreateNow(false);

    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        CHECK(rhi.RHIDrainDeferredCreates() == 1);
    }

    // The request that could not create keeps its place in the queue; the one behind it is not
    // held back by it.
    CHECK(!blocked->IsReady());
    CHECK(ready->IsReady());
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 1);
    CHECK(rhi.GetDeferredCreateQueue().GetRetriedCount() == 1);
    CHECK(log->GetSequence() == std::vector<int>{ 1 });

    blocked->SetCanCreateNow(true);
    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        CHECK(rhi.RHIDrainDeferredCreates() == 1);
    }

    CHECK(blocked->IsReady());
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 0);
    CHECK(log->GetSequence() == std::vector<int>{ 1, 0 });

    threading.Shutdown();
}
TEST_CASE("rhi-deferred-create: a shader created off the graphics role queues its program [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    const std::shared_ptr<RHIShader> shader = rhi.RHICreateShader(RHIShaderCreateDesc{}, nullptr);

    REQUIRE(shader != nullptr);
    // The handle is complete; the program is not, and the create call did not touch the driver.
    CHECK(!shader->IsReady());
    CHECK(!shader->IsValid());
    CHECK(!shader->HasNativeCreateFailed());
    CHECK(rhi.GetProgramLog()->GetCreateCount() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 1);
    CHECK(rhi.GetDeferredCreateQueue().GetPostedCount() == 1);

    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: the frame-start drain creates a queued program on the graphics role [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    const std::shared_ptr<RHIShader> shader = rhi.RHICreateShader(RHIShaderCreateDesc{}, nullptr);
    REQUIRE(shader != nullptr);
    REQUIRE(!shader->IsReady());

    std::atomic<std::thread::id> graphicsThreadId{};
    tasks.StartFrameRoles(
        [&rhi, &graphicsThreadId](float) {
            graphicsThreadId.store(std::this_thread::get_id(), std::memory_order_release);
            rhi.RHIDrainDeferredCreates();
        },
        []() {},
        /*combineGraphicsThreads=*/true);

    tasks.SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);

    CHECK(shader->IsReady());
    CHECK(shader->IsValid());
    CHECK(rhi.GetProgramLog()->GetCreateCount() == 1);
    CHECK(rhi.GetProgramLog()->GetThreadOfCreate() == graphicsThreadId.load());
    CHECK(rhi.GetProgramLog()->GetThreadOfCreate() != std::this_thread::get_id());
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetCreatedCount() == 1);

    tasks.Shutdown();
    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: a shader created on the graphics role is created at once [smoke]")
{
    // Single topology binds every role to one thread, so the caller already is the graphics role
    // and a program may not wait for a frame start it does not need.
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Single);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    const std::shared_ptr<RHIShader> shader = rhi.RHICreateShader(RHIShaderCreateDesc{}, nullptr);

    REQUIRE(shader != nullptr);
    CHECK(shader->IsReady());
    CHECK(shader->IsValid());
    CHECK(rhi.GetProgramLog()->GetThreadOfCreate() == std::this_thread::get_id());
    CHECK(rhi.GetDeferredCreateQueue().GetPostedCount() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 0);

    // A pipeline state that owns no driver object is ready where it is built, so the channel
    // passes it through instead of queueing a frame-start no-op.
    const std::shared_ptr<RHIGraphicsPipelineState> description =
        rhi.RHICreateGraphicsPipelineState(RHIGraphicsPSODesc{});
    REQUIRE(description != nullptr);
    CHECK(description->IsReady());
    CHECK(rhi.GetDeferredCreateQueue().GetPostedCount() == 0);

    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: pipeline state, layouts, binding set and sampler follow the same channel [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);
    // This probe models a backend whose pipeline state does own a driver object.
    rhi.SetPipelineStateOwnsNativeObject(true);

    const std::shared_ptr<RHIGraphicsPipelineState> pipelineState =
        rhi.RHICreateGraphicsPipelineState(RHIGraphicsPSODesc{});
    const std::shared_ptr<RHIShaderBindingSetLayout> layout =
        rhi.RHICreateShaderBindingSetLayout({});
    const std::shared_ptr<RHIShaderBindingSet> bindingSet =
        rhi.RHICreateShaderBindingSet(nullptr, {});
    const std::shared_ptr<RHISampler> sampler = rhi.RHIGetOrCreateSampler(RHISamplerDesc{});

    REQUIRE(pipelineState != nullptr);
    REQUIRE(layout != nullptr);
    REQUIRE(bindingSet != nullptr);
    REQUIRE(sampler != nullptr);
    CHECK(!pipelineState->IsReady());
    CHECK(!layout->IsReady());
    CHECK(!bindingSet->IsReady());
    CHECK(!sampler->IsReady());
    CHECK(rhi.GetProgramLog()->GetCreateCount() == 0);
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 4);

    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        CHECK(rhi.RHIDrainDeferredCreates() == 4);
    }

    CHECK(pipelineState->IsReady());
    CHECK(layout->IsReady());
    CHECK(bindingSet->IsReady());
    CHECK(sampler->IsReady());
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 0);
    const std::vector<std::string> expected{
        "probe pipeline state",
        "probe binding set layout",
        "probe binding set",
        "probe sampler",
    };
    CHECK(rhi.GetProgramLog()->GetCreatedKinds() == expected);

    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: a program that fails on the graphics role is reported once and not retried [smoke]")
{
    EnsureLogSinkInstalled();

    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);

    const std::shared_ptr<ProbeShader> shader = rhi.CreateProbeShader();
    REQUIRE(shader != nullptr);
    // The injected failure: this backend's driver will refuse the program, the way a driver
    // refuses bytecode it cannot specialize.
    shader->SetShouldFail(true);
    REQUIRE(rhi.GetDeferredCreateQueue().GetPendingCount() == 1);

    GetSharedLogSink().Arm("the driver refused the program");
    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        CHECK(rhi.RHIDrainDeferredCreates() == 0);
    }
    const uint32_t failureReports = GetSharedLogSink().Disarm();

    // One report, at the moment and on the thread that saw the driver's answer.
    CHECK(failureReports == 1);
    CHECK(shader->HasNativeCreateFailed());
    CHECK(!shader->IsValid());
    CHECK(!shader->IsReady());
    CHECK(!shader->GetCompileLog().empty());
    CHECK(rhi.GetDeferredCreateQueue().GetFailedCount() == 1);
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 0);

    // Refused for good: the next frame start asks the driver nothing and reports nothing.
    GetSharedLogSink().Arm("the driver refused the program");
    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        CHECK(rhi.RHIDrainDeferredCreates() == 0);
    }
    CHECK(GetSharedLogSink().Disarm() == 0);
    CHECK(rhi.GetProgramLog()->GetRefusedKinds().size() == 1);

    threading.Shutdown();
}

TEST_CASE("rhi-deferred-create: a queue that never reaches a frame start warns about its stall [smoke]")
{
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const std::shared_ptr<NativeCreateLog> log = std::make_shared<NativeCreateLog>();
    HeadlessRHI rhi(log);
    rhi.GetDeferredCreateQueue().SetStallWarnIntervalMs(1);

    const std::shared_ptr<RHIBuffer> first = rhi.RHICreateBuffer(MakeProbeBufferDesc(), nullptr);
    REQUIRE(!first->IsReady());
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const std::shared_ptr<RHIBuffer> second = rhi.RHICreateBuffer(MakeProbeBufferDesc(), nullptr);
    REQUIRE(!second->IsReady());

    // The requests are still there and still usable - the next frame start creates them - but a
    // loop that never gets one now says so instead of leaving them silently queued.
    CHECK(rhi.GetDeferredCreateQueue().GetStallWarnCount() >= 1);
    CHECK(rhi.GetDeferredCreateQueue().GetOldestPendingAgeMs() >= 5);
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() == 2);

    threading.Shutdown();
}

