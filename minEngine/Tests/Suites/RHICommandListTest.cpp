#include "RHICommandListTest.h"

#include "Runtime/Function/Render/EngineRHITextureUtils.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHIResources.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHICommandListPool.h"
#include "Runtime/Function/Render/RHI/RHIRenderPass.h"

#include "doctest.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using minEngine::ERHICommandListState;
using minEngine::MakeUniformBufferDesc;
using minEngine::OpenGLRHI;
using minEngine::RHI;
using minEngine::RHIBufferRef;
using minEngine::RHICommandListPool;
using minEngine::RHICommandList;
using minEngine::RHIRenderPassInfo;

namespace
{
    class CountingRHI final : public RHI
    {
    public:
        int BeginPassCount = 0;
        int EndPassCount = 0;
        int DrawCount = 0;
        /** Stands where the driver would read a bound buffer: runs at Replay, in command order. */
        std::function<void()> OnDraw;

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

        void RHICmdBeginRenderPass(const RHIRenderPassInfo&) override { ++BeginPassCount; }
        void RHICmdEndRenderPass() override { ++EndPassCount; }
        void RHICmdSetGraphicsPipelineState(minEngine::RHIGraphicsPipelineState*) override {}
        void RHICmdSetShaderBindingSet(uint32_t, minEngine::RHIShaderBindingSet*) override {}
        void RHICmdTransition(const minEngine::RHITextureTransitionInfo&) override {}
        void RHICmdSetViewport(uint32_t, uint32_t, uint32_t, uint32_t, bool) override {}
        void RHICmdSetVertexBuffer(minEngine::RHIBuffer*, uint32_t) override {}
        void RHICmdSetIndexBuffer(minEngine::RHIBuffer*) override {}
        void RHICmdDrawIndexed(uint32_t, uint32_t, int32_t) override { RunDraw(); }
        void RHICmdDraw(uint32_t, uint32_t) override { RunDraw(); }
        void RHICmdGenerateMips(minEngine::RHITexture*) override {}
        void RHISetBackbufferClearColor(const minEngine::Math::Vector3&) override {}
        void RHIClearBackbuffer() override {}
        void RHIPresent() override {}

    private:
        void RunDraw()
        {
            ++DrawCount;
            if (OnDraw)
            {
                OnDraw();
            }
        }
    };

    /** Headless context: the bypass check needs the real backend primitive, not a test double. */
    class ScopedHeadlessGlContext
    {
    public:
        ScopedHeadlessGlContext()
        {
            if (glfwGetCurrentContext() != nullptr)
            {
                return;
            }

            if (!glfwInit())
            {
                return;
            }

            m_OwnsGlfw = true;
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            m_Window = glfwCreateWindow(1, 1, "RHICommandList", nullptr, nullptr);
            if (m_Window == nullptr)
            {
                return;
            }

            glfwMakeContextCurrent(m_Window);
            if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
            {
                glfwDestroyWindow(m_Window);
                m_Window = nullptr;
            }
        }

        ~ScopedHeadlessGlContext()
        {
            if (m_Window != nullptr)
            {
                glfwDestroyWindow(m_Window);
                m_Window = nullptr;
            }
            if (m_OwnsGlfw)
            {
                glfwTerminate();
            }
        }

        ScopedHeadlessGlContext(const ScopedHeadlessGlContext&) = delete;
        ScopedHeadlessGlContext& operator=(const ScopedHeadlessGlContext&) = delete;

        bool IsReady() const { return glfwGetCurrentContext() != nullptr; }

    private:
        GLFWwindow* m_Window = nullptr;
        bool m_OwnsGlfw = false;
    };

    /**
     * A buffer whose bytes can be read back, standing in for the backend primitive: the payload a
     * write carries is visible only once that write actually runs.
     */
    class TrackingBuffer final : public minEngine::RHIBuffer
    {
    public:
        explicit TrackingBuffer(uint32_t byteSize)
            : m_Bytes(byteSize, 0u)
        {
            m_Desc.Usage = minEngine::RHIBufferUsage::Uniform;
            m_Desc.ByteSize = byteSize;
        }

        const minEngine::RHIBufferCreateDesc& GetDesc() const override { return m_Desc; }

        void UpdateSubresource(const void* data, uint32_t offset, uint32_t size) override
        {
            if (data == nullptr || size == 0 || offset + size > m_Bytes.size())
            {
                return;
            }
            std::memcpy(m_Bytes.data() + offset, data, size);
        }

        std::string Text() const { return std::string(m_Bytes.begin(), m_Bytes.end()); }

    private:
        minEngine::RHIBufferCreateDesc m_Desc;
        std::vector<unsigned char> m_Bytes;
    };
}

TEST_CASE("rhi-command-list: immediate is default without Multi [smoke]")
{
    CountingRHI rhi;
    RHICommandList cmdList(&rhi);
    CHECK(cmdList.IsImmediate());
    CHECK(cmdList.GetState() == ERHICommandListState::Idle);

    cmdList.BeginRecording();
    cmdList.EndRenderPass();
    CHECK(rhi.EndPassCount == 1);
    CHECK(cmdList.GetState() == ERHICommandListState::Idle);

    cmdList.FinishRecording();
    cmdList.Replay();
    CHECK(rhi.EndPassCount == 1);
}

TEST_CASE("rhi-command-list: recording defers until Replay [smoke]")
{
    CountingRHI rhi;
    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(false);
    CHECK_FALSE(cmdList.IsImmediate());

    cmdList.BeginRecording();
    CHECK(cmdList.GetState() == ERHICommandListState::Recording);

    RHIRenderPassInfo info{};
    cmdList.BeginRenderPass(info);
    cmdList.Draw(3);
    cmdList.EndRenderPass();
    CHECK(rhi.BeginPassCount == 0);
    CHECK(rhi.DrawCount == 0);
    CHECK(rhi.EndPassCount == 0);

    cmdList.FinishRecording();
    CHECK(cmdList.GetState() == ERHICommandListState::Finished);

    cmdList.Replay();
    CHECK(rhi.BeginPassCount == 1);
    CHECK(rhi.DrawCount == 1);
    CHECK(rhi.EndPassCount == 1);
    CHECK(cmdList.GetState() == ERHICommandListState::Idle);
}
TEST_CASE("rhi-command-list: pool lease returns the list when the frame bails out early [smoke]")
{
    CountingRHI rhi;
    RHICommandListPool pool;
    pool.Initialize(&rhi);

    // Simulates a frame returning before the recording is submitted (setup failure): the pool only
    // holds kMaxInFlightCommandLists lists, so three leaked borrows would block every later frame.
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        RHICommandListPool::Lease lease = pool.AcquireLease();
        CHECK(lease.IsValid());
        CHECK(lease.Get() != nullptr);
        CHECK(lease->IsImmediate() == false);
    }

    // The fourth borrow must still get a list; the wait is bounded so a leak fails the case instead
    // of wedging the binary.
    std::atomic<bool> borrowed{false};
    std::thread borrower([&pool, &borrowed]() {
        RHICommandListPool::Lease lease = pool.AcquireLease();
        borrowed.store(lease.IsValid(), std::memory_order_release);
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!borrowed.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!borrowed.load(std::memory_order_acquire))
    {
        pool.RequestShutdown();
    }
    borrower.join();
    CHECK(borrowed.load(std::memory_order_acquire));

    {
        RHICommandListPool::Lease lease = pool.AcquireLease();
        REQUIRE(lease.IsValid());
        lease->BeginRecording();
        lease->FinishRecording();
        // Submitting hands the recording to the RHI side; the lease must not also free the slot.
        lease.SubmitFinished();
        CHECK_FALSE(lease.IsValid());
    }

    RHICommandList* finished = pool.TryTakeFinished();
    REQUIRE(finished != nullptr);
    CHECK(pool.TryTakeFinished() == nullptr);
    pool.Release(finished);

    pool.Shutdown();
}

TEST_CASE("rhi-command-list: a draw recorded between two uploads reads the first [full]")
{
    CountingRHI rhi;
    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(false);

    TrackingBuffer buffer(4u);
    const char first[] = "AAAA";
    const char second[] = "BBBB";
    std::string bytesAtDraw;

    rhi.OnDraw = [&]() { bytesAtDraw = buffer.Text(); };

    cmdList.BeginRecording();
    cmdList.UpdateBuffer(&buffer, first, 0, 4u);
    cmdList.Draw(3u);
    cmdList.UpdateBuffer(&buffer, second, 0, 4u);
    cmdList.FinishRecording();

    // A recording runs nothing yet, so it has written nothing yet either.
    CHECK(rhi.DrawCount == 0);
    CHECK(buffer.Text() == std::string(4u, '\0'));

    cmdList.Replay();

    CHECK(rhi.DrawCount == 1);
    // The draw sits between the two uploads in the stream, so it reads the first payload.
    CHECK(bytesAtDraw == "AAAA");
    CHECK(buffer.Text() == "BBBB");
}

TEST_CASE("rhi-command-list: a recorded upload keeps the payload it was given [full]")
{
    CountingRHI rhi;
    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(false);

    TrackingBuffer buffer(4u);
    std::string bytesAtDraw;
    rhi.OnDraw = [&]() { bytesAtDraw = buffer.Text(); };

    std::vector<char> payload = { 'A', 'A', 'A', 'A' };
    cmdList.BeginRecording();
    cmdList.UpdateBuffer(&buffer, payload.data(), 0, 4u);
    // The caller owns that memory and is free to reuse it as soon as the call returns.
    std::fill(payload.begin(), payload.end(), 'Z');
    cmdList.Draw(3u);
    cmdList.FinishRecording();
    cmdList.Replay();

    CHECK(bytesAtDraw == "AAAA");
}

TEST_CASE("rhi-command-list: an upload outside a recording still lands immediately [full]")
{
    CountingRHI rhi;
    RHICommandList cmdList(&rhi);
    REQUIRE(cmdList.IsImmediate());

    TrackingBuffer buffer(4u);
    const uint64_t bypassesBefore = RHICommandList::GetImmediateUploadWhileRecordingCount();

    const char payload[] = "AAAA";
    cmdList.UpdateBuffer(&buffer, payload, 0, 4u);

    // Immediate mode keeps the behaviour it always had: applied when the call returns, with no
    // copy and no replay step to wait for.
    CHECK(buffer.Text() == "AAAA");
    CHECK(cmdList.GetState() == ERHICommandListState::Idle);
    CHECK(RHICommandList::GetImmediateUploadWhileRecordingCount() == bypassesBefore);
}

TEST_CASE("rhi-command-list: only a write that bypasses the recording is reported [full]")
{
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());

    // Real GL buffer behind a real backend primitive, so the check covers the wiring and not just
    // this suite's idea of it.
    OpenGLRHI rhi;
    RHICommandList cmdList(&rhi);
    RHIBufferRef buffer = cmdList.CreateBuffer(MakeUniformBufferDesc(64u));
    REQUIRE(buffer);

    float payload[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    const uint64_t bypassesBefore = RHICommandList::GetImmediateUploadWhileRecordingCount();

    // Immediate mode: the write is the frame's own and lands where the caller asked for it.
    cmdList.UpdateBuffer(buffer.get(), payload, 0, sizeof(payload));
    CHECK(RHICommandList::GetImmediateUploadWhileRecordingCount() == bypassesBefore);

    cmdList.SetImmediateOverride(false);
    cmdList.BeginRecording();
    cmdList.UpdateBuffer(buffer.get(), payload, 0, sizeof(payload));
    CHECK(RHICommandList::GetImmediateUploadWhileRecordingCount() == bypassesBefore);

    // The primitive called straight through while the recording is open: it lands before the
    // draws that recording holds, so it is reported.
    buffer->UpdateSubresource(payload, 0, sizeof(payload));
    CHECK(RHICommandList::GetImmediateUploadWhileRecordingCount() == bypassesBefore + 1);

    cmdList.FinishRecording();
    // Replay runs the recorded upload with the window already closed: a write there belongs to
    // this stream and is not a bypass.
    cmdList.Replay();
    CHECK(RHICommandList::GetImmediateUploadWhileRecordingCount() == bypassesBefore + 1);
}

