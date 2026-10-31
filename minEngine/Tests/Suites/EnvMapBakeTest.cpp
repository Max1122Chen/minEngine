#include "EnvMapBakeTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/Environment/EnvMapCapture.h"
#include "Runtime/Function/Render/Environment/EnvironmentMap.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIRenderPass.h"

#include "doctest.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using minEngine::EEnvMapCaptureStatus;
using minEngine::EEnvironmentBakeAdmission;
using minEngine::EnvironmentBakeSchedule;
using minEngine::FrameThreadMode;
using minEngine::FrameThreading;
using minEngine::GetEnvironmentBakeAdmission;
using minEngine::LogSystem;
using minEngine::LogRecord;
using minEngine::RHI;
using minEngine::RHICommandList;
using minEngine::RHITexture;
using minEngine::RHITextureCreateDesc;
using minEngine::RHITextureRef;
using minEngine::TaskSystem;
using minEngine::TextureCube;

namespace
{
    /** A texture handle whose native half the device claims to have, or to still owe. */
    class FakeTexture final : public RHITexture
    {
    public:
        FakeTexture(const RHITextureCreateDesc& desc, bool bReady)
            : m_Desc(desc)
            , m_bReady(bReady)
        {
        }

        const RHITextureCreateDesc& GetDesc() const override { return m_Desc; }
        /** Stands for a native object the graphics role has not created yet. */
        bool IsReady() const override { return m_bReady; }

    private:
        RHITextureCreateDesc m_Desc;
        bool m_bReady = true;
    };

    class FakeShader final : public minEngine::RHIShader
    {
    public:
        bool IsValid() const override { return true; }
        const std::string& GetCompileLog() const override { return m_Log; }

    private:
        std::string m_Log;
    };

    /** Geometry the bake's draw names: the device hands out complete handles, as a role does. */
    class FakeBuffer final : public minEngine::RHIBuffer
    {
    public:
        explicit FakeBuffer(const minEngine::RHIBufferCreateDesc& desc)
            : m_Desc(desc)
        {
        }

        const minEngine::RHIBufferCreateDesc& GetDesc() const override { return m_Desc; }
        void UpdateSubresource(const void*, uint32_t, uint32_t) override {}

    private:
        minEngine::RHIBufferCreateDesc m_Desc;
    };

    /** The vertex state the cube is read through; complete, like a layout created on the role. */
    class FakeVertexInputLayout final : public minEngine::RHIVertexInputLayout
    {
    public:
        const std::vector<minEngine::RHIVertexElement>& GetElements() const override { return m_Elements; }
        uint32_t GetStride() const override { return 12u; }

    private:
        std::vector<minEngine::RHIVertexElement> m_Elements{ { "a_Position", minEngine::VertexElementType::Float3 } };
    };

    /**
     * A pipeline state that carries the description it was created from.
     *
     * The draw path reads the description back to answer which vertex state the draw runs with, so a
     * double that cannot produce one would make every draw unanswerable rather than testable.
     */
    class FakePipelineState final : public minEngine::RHIGraphicsPSOStateFallback
    {
    public:
        explicit FakePipelineState(const minEngine::RHIGraphicsPSODesc& desc)
            : minEngine::RHIGraphicsPSOStateFallback(desc)
        {
        }
    };

    /**
     * A device that answers with handles and records what reached it.
     *
     * The bake's failure mode was that its commands never reached any device at all, so a double
     * that counts the commands it was called with is the observation the test needs: a cubemap with
     * no draws behind it and a cubemap with six are the same pointer here and different content.
     */
    class BakeDeviceRHI final : public RHI
    {
    public:
        struct RecordedFace
        {
            RHITexture* RenderTarget = nullptr;
            int32_t ArraySlice = -1;
            uint8_t MipIndex = 0;
        };

        int BeginPassCount = 0;
        int EndPassCount = 0;
        int DrawCount = 0;
        int GenerateMipsCount = 0;
        std::vector<RecordedFace> Faces;
        std::vector<RHITexture*> MipTargets;
        /** Handles the device hands out from here on answer IsReady() with this. */
        bool bTexturesReady = true;

        void Initialize() override {}
        void Shutdown() override {}

        std::shared_ptr<RHITexture> RHICreateTexture2D(
            const RHITextureCreateDesc& desc,
            const void*) override
        {
            return std::make_shared<FakeTexture>(desc, bTexturesReady);
        }
        std::shared_ptr<minEngine::RHIShaderResourceView> RHICreateShaderResourceView(
            const minEngine::RHITextureSRVDesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIBuffer> RHICreateBuffer(
            const minEngine::RHIBufferCreateDesc& desc,
            const void*) override
        {
            return std::make_shared<FakeBuffer>(desc);
        }
        std::shared_ptr<minEngine::RHIShader> RHICreateShader(
            const minEngine::RHIShaderCreateDesc&,
            std::string*) override
        {
            return std::make_shared<FakeShader>();
        }
        std::shared_ptr<minEngine::RHIShader> RHICreateShader(
            const std::string&,
            const std::string&,
            std::string*) override
        {
            return std::make_shared<FakeShader>();
        }
        std::shared_ptr<minEngine::RHIGraphicsPipelineState> RHICreateGraphicsPipelineState(
            const minEngine::RHIGraphicsPSODesc& desc) override
        {
            return std::make_shared<FakePipelineState>(desc);
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
            return std::make_shared<FakeVertexInputLayout>();
        }

        void RHICmdBeginRenderPass(const minEngine::RHIRenderPassInfo& info) override
        {
            ++BeginPassCount;
            RecordedFace face;
            face.RenderTarget = info.ColorAttachments[0].RenderTarget;
            face.ArraySlice = info.ColorAttachments[0].ArraySlice;
            face.MipIndex = info.ColorAttachments[0].MipIndex;
            Faces.push_back(face);
        }
        void RHICmdEndRenderPass() override { ++EndPassCount; }
        void RHICmdSetGraphicsPipelineState(minEngine::RHIGraphicsPipelineState*) override {}
        void RHICmdSetShaderBindingSet(uint32_t, minEngine::RHIShaderBindingSet*) override {}
        void RHICmdTransition(const minEngine::RHITextureTransitionInfo&) override {}
        void RHICmdSetViewport(uint32_t, uint32_t, uint32_t, uint32_t, bool) override {}
        void RHICmdSetVertexBuffer(minEngine::RHIBuffer*, uint32_t) override {}
        void RHICmdSetIndexBuffer(minEngine::RHIBuffer*) override {}
        void RHICmdDrawIndexed(uint32_t, uint32_t, int32_t) override {}
        void RHICmdDraw(uint32_t, uint32_t) override { ++DrawCount; }
        void RHICmdGenerateMips(RHITexture* texture) override
        {
            ++GenerateMipsCount;
            MipTargets.push_back(texture);
        }
        void RHISetBackbufferClearColor(const minEngine::Math::Vector3&) override {}
        void RHIClearBackbuffer() override {}
        void RHIPresent() override {}

    };

    /**
     * LogSystem keeps every sink it is given for the life of the process, so the recorder is shared
     * instead of owned by a case: a case that dropped it would leave the log writing into freed
     * memory for every suite that runs after it.
     */
    struct RecordingSink final : public minEngine::ILogSink
    {
        std::mutex Mutex;
        std::vector<std::string> Messages;

        void Write(const LogRecord& record) override
        {
            std::lock_guard<std::mutex> lock(Mutex);
            Messages.push_back(record.message);
        }
    };

    std::shared_ptr<RecordingSink> GetRecordingSink()
    {
        static std::shared_ptr<RecordingSink> s_Sink = []()
        {
            std::shared_ptr<RecordingSink> sink = std::make_shared<RecordingSink>();
            LogSystem::AddSink(sink);
            return sink;
        }();
        return s_Sink;
    }

    bool HasMessageWith(RecordingSink& sink, const std::string& needle)
    {
        std::lock_guard<std::mutex> lock(sink.Mutex);
        for (const std::string& message : sink.Messages)
        {
            if (message.find(needle) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    /** Puts the process in the recording topology a Multi frame runs in. */
    struct MultiRecordingTopology
    {
        FrameThreading Threading;
        TaskSystem Tasks;

        MultiRecordingTopology()
        {
            Threading.Initialize(FrameThreadMode::Multi);
            Tasks.Init(2);
            // The engine installs the instance it created; a command list asks the installed one
            // whether the frame roles run, so a test that only called Init would keep running in
            // the immediate topology and would test nothing.
            TaskSystem::SetInstance(&Tasks);
            Tasks.StartFrameRoles([](float) {}, []() {}, /*combineGraphicsThreads=*/true);
        }

        ~MultiRecordingTopology()
        {
            Tasks.StopFrameRoles();
            TaskSystem::SetInstance(nullptr);
            Tasks.Shutdown();
            Threading.Shutdown();
        }

        MultiRecordingTopology(const MultiRecordingTopology&) = delete;
        MultiRecordingTopology& operator=(const MultiRecordingTopology&) = delete;
    };

    /** Single topology: the calling thread owns every role, so it is the graphics role. */
    struct SingleTopology
    {
        FrameThreading Threading;

        SingleTopology() { Threading.Initialize(FrameThreadMode::Single); }
        ~SingleTopology() { Threading.Shutdown(); }

        SingleTopology(const SingleTopology&) = delete;
        SingleTopology& operator=(const SingleTopology&) = delete;
    };

    /**
     * The bake reads its programs from the engine's shipped assets. The test binary runs from the
     * build's bin directory, so the root is found relative to it; nothing here invents a shader.
     */
    std::filesystem::path ResolveEngineDefaultAssetsRoot()
    {
        const std::filesystem::path candidates[] = {
            "../Assets/EngineDefault",
            "Assets/EngineDefault",
            "../../minEngine/Assets/EngineDefault",
            "../minEngine/Assets/EngineDefault",
            "minEngine/Assets/EngineDefault",
        };

        for (const std::filesystem::path& candidate : candidates)
        {
            if (std::filesystem::exists(candidate / "Shaders" / "EnvMap" / "equirect_to_cubemap.vert"))
            {
                return std::filesystem::weakly_canonical(candidate);
            }
        }
        return {};
    }
}

TEST_CASE("envmap-bake: a command issued outside a recording window is refused and named [smoke]")
{
    BakeDeviceRHI device;
    RHICommandList cmdList(&device);
    cmdList.SetImmediateOverride(false);
    REQUIRE_FALSE(cmdList.IsImmediate());

    std::shared_ptr<RecordingSink> sink = GetRecordingSink();
    {
        std::lock_guard<std::mutex> lock(sink->Mutex);
        sink->Messages.clear();
    }

    // The defect this replaces asserted here, which took the session down in a debug build and
    // queued a command nobody replayed in a release one.
    cmdList.Draw(3u);
    cmdList.EndRenderPass();

    // Neither command reached the device and neither was queued for a replay that would run it out
    // of order; the list is still Idle and the frame that follows can record as usual.
    CHECK(cmdList.GetState() == minEngine::ERHICommandListState::Idle);
    CHECK(device.DrawCount == 0);
    CHECK(device.EndPassCount == 0);
    CHECK(device.BeginPassCount == 0);

    // The line names the caller, not just the rule: a reader has to be able to find the site.
    CHECK(HasMessageWith(*sink, "command refused"));
    CHECK(HasMessageWith(*sink, "role="));
    CHECK(HasMessageWith(*sink, "caller:"));
    CHECK(HasMessageWith(*sink, "command(s) refused so far"));

    // Inside the window the same calls reach the device at Replay.
    cmdList.BeginRecording();
    cmdList.BeginRenderPass(minEngine::RHIRenderPassInfo{});
    cmdList.Draw(3u);
    cmdList.EndRenderPass();
    cmdList.FinishRecording();
    CHECK(device.DrawCount == 0);
    cmdList.Replay();
    CHECK(device.DrawCount == 1);
    CHECK(device.BeginPassCount == 1);
}

TEST_CASE("envmap-bake: a bake is admitted on the graphics role and outside a recording window [smoke]")
{
    {
        SingleTopology topology;
        CHECK(GetEnvironmentBakeAdmission() == EEnvironmentBakeAdmission::Allowed);

        BakeDeviceRHI device;
        RHICommandList cmdList(&device);
        cmdList.SetImmediateOverride(false);
        CHECK_FALSE(RHICommandList::IsRecordingWindowOpenOnThisThread());

        cmdList.BeginRecording();
        // A bake here would submit its own command buffer into the stream this list is building.
        CHECK(RHICommandList::IsRecordingWindowOpenOnThisThread());
        CHECK(GetEnvironmentBakeAdmission() == EEnvironmentBakeAdmission::RecordingWindowOpen);

        cmdList.FinishRecording();
        cmdList.Replay();
        CHECK_FALSE(RHICommandList::IsRecordingWindowOpenOnThisThread());
        CHECK(GetEnvironmentBakeAdmission() == EEnvironmentBakeAdmission::Allowed);
    }

    {
        // Multi with the frame roles running: this thread is the game role, and a bake there would
        // create textures without a graphics context.
        MultiRecordingTopology topology;
        CHECK(GetEnvironmentBakeAdmission() == EEnvironmentBakeAdmission::NotGraphicsRole);
    }
}

TEST_CASE("envmap-bake: a failed bake backs off instead of repeating on every frame [full]")
{
    EnvironmentBakeSchedule schedule;
    CHECK(schedule.BeginFrame() == EnvironmentBakeSchedule::EDecision::Attempt);

    // A handle that is still queued: the failure is worth retrying, but not on the next frame.
    schedule.NoteAttemptFailed(/*bRetryable=*/true);
    CHECK(schedule.GetAttempts() == 1u);
    CHECK(schedule.GetQuietFrames() == EnvironmentBakeSchedule::kFirstQuietFrames);

    for (uint32_t frame = 0; frame < EnvironmentBakeSchedule::kFirstQuietFrames; ++frame)
    {
        CHECK(schedule.BeginFrame() == EnvironmentBakeSchedule::EDecision::Defer);
    }
    CHECK(schedule.BeginFrame() == EnvironmentBakeSchedule::EDecision::Attempt);

    // The same failure again buys a longer quiet period, and the period has a ceiling so a resource
    // that arrives late still lands.
    schedule.NoteAttemptFailed(true);
    CHECK(schedule.GetQuietFrames() == EnvironmentBakeSchedule::kFirstQuietFrames * 2u);
    CHECK_FALSE(schedule.IsSettled());

    uint32_t quietFrames = schedule.GetQuietFrames();
    for (uint32_t attempt = 0; attempt < 8; ++attempt)
    {
        for (uint32_t frame = 0; frame < quietFrames; ++frame)
        {
            REQUIRE(schedule.BeginFrame() == EnvironmentBakeSchedule::EDecision::Defer);
        }
        REQUIRE(schedule.BeginFrame() == EnvironmentBakeSchedule::EDecision::Attempt);
        schedule.NoteAttemptFailed(true);
        quietFrames = schedule.GetQuietFrames();
        CHECK(quietFrames <= EnvironmentBakeSchedule::kMaxQuietFrames);
    }
    CHECK_FALSE(schedule.IsSettled());

    // A refusal is final for this object: it settles instead of baking again on its own.
    EnvironmentBakeSchedule refused;
    refused.NoteAttemptFailed(/*bRetryable=*/false);
    CHECK(refused.IsSettled());
    CHECK(refused.BeginFrame() == EnvironmentBakeSchedule::EDecision::Settled);
    CHECK(refused.BeginFrame() == EnvironmentBakeSchedule::EDecision::Settled);
}
TEST_CASE("envmap-bake: a Multi bake draws six faces into the cubemap instead of nothing [full]")
{
    const std::filesystem::path assetsRoot = ResolveEngineDefaultAssetsRoot();
    REQUIRE_MESSAGE(
        !assetsRoot.empty(),
        "engine default assets holding Shaders/EnvMap were not found next to the test binary");

    // The topology the bake crashed in: a list handed an Idle state must record, not assert.
    MultiRecordingTopology topology;

    BakeDeviceRHI device;
    RHICommandList probe(&device);
    REQUIRE_FALSE(probe.IsImmediate());

    RHITextureCreateDesc sourceDesc;
    sourceDesc.Width = 8;
    sourceDesc.Height = 8;
    sourceDesc.Format = minEngine::TextureFormat::RGBA16F;
    RHITextureRef source = device.RHICreateTexture2D(sourceDesc, nullptr);
    REQUIRE(source);

    std::string error;
    EEnvMapCaptureStatus status = EEnvMapCaptureStatus::Failed;
    std::shared_ptr<TextureCube> cube = minEngine::EnvMapCapture::EquirectToCubemap(
        device,
        *source,
        assetsRoot,
        8u,
        &error,
        &status);

    REQUIRE_MESSAGE(cube != nullptr, error);
    CHECK(status == EEnvMapCaptureStatus::Succeeded);

    // The cubemap is handed back only after its six faces ran: an empty bake reported success
    // because its commands were queued and dropped, which is what the device count proves now.
    REQUIRE(device.Faces.size() == 6u);
    for (uint32_t faceIndex = 0; faceIndex < 6u; ++faceIndex)
    {
        CHECK(device.Faces[faceIndex].RenderTarget == cube->GetRHITexture());
        CHECK(device.Faces[faceIndex].ArraySlice == static_cast<int32_t>(faceIndex));
        CHECK(device.Faces[faceIndex].MipIndex == 0u);
    }
    CHECK(device.DrawCount == 6);
    CHECK(device.EndPassCount == 6);
    REQUIRE(device.MipTargets.size() == 1u);
    CHECK(device.MipTargets[0] == cube->GetRHITexture());
}

TEST_CASE("envmap-bake: a bake refuses handles with no native object instead of drawing into them [full]")
{
    const std::filesystem::path assetsRoot = ResolveEngineDefaultAssetsRoot();
    REQUIRE_MESSAGE(
        !assetsRoot.empty(),
        "engine default assets holding Shaders/EnvMap were not found next to the test binary");

    MultiRecordingTopology topology;

    BakeDeviceRHI device;
    // The device answers with handles whose native half is only queued.
    device.bTexturesReady = false;

    RHITextureCreateDesc sourceDesc;
    sourceDesc.Width = 8;
    sourceDesc.Height = 8;
    sourceDesc.Format = minEngine::TextureFormat::RGBA16F;
    RHITextureRef source = device.RHICreateTexture2D(sourceDesc, nullptr);
    REQUIRE(source);

    std::string error;
    EEnvMapCaptureStatus status = EEnvMapCaptureStatus::Succeeded;
    std::shared_ptr<TextureCube> cube = minEngine::EnvMapCapture::EquirectToCubemap(
        device,
        *source,
        assetsRoot,
        8u,
        &error,
        &status);

    CHECK(cube == nullptr);
    // Queued is a retry, not a failure: the caller is told which one it is.
    CHECK(status == EEnvMapCaptureStatus::RetryLater);
    CHECK(error.find("not created yet") != std::string::npos);
    CHECK(device.Faces.empty());
    CHECK(device.DrawCount == 0);
}
