#include "AudioSmokeTest.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"
#include "Access/AudioSystemTestAccess.h"

#include "MockAudioBackend.h"

#include "Runtime/Function/Audio/AudioClipLease.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Audio/AudioSystem.h"
#include "Runtime/Function/Audio/Components/AudioComponent.h"
#include "Runtime/Function/Audio/Components/AudioListenerComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"
#include "Runtime/Resource/AudioClip.h"

#include <cmath>
#include <memory>
#include <vector>

namespace minEngine
{
    class AudioSmokeTestScope
    {
    public:
        AudioSmokeTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            Testing::TestAccess<AudioSystem>::SetInstance(&m_AudioSystem);
            Testing::TestAccess<AudioSystem>::InitializeWithBackend(
                m_AudioSystem,
                std::make_unique<MockAudioBackend>());
        }

        ~AudioSmokeTestScope()
        {
            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);

            m_AudioSystem.Shutdown();
            Testing::TestAccess<AudioSystem>::SetInstance(nullptr);

            ObjectManager::Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

        MockAudioBackend* GetBackend() const
        {
            return static_cast<MockAudioBackend*>(m_AudioSystem.GetBackend());
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
        AudioSystem m_AudioSystem;
    };

    namespace
    {
        std::shared_ptr<AudioClip> CreateTestToneClip(float frequencyHz = 440.0f, float durationSeconds = 0.25f)
        {
            constexpr uint32_t sampleRate = 44100;
            const uint64_t frameCount =
                static_cast<uint64_t>(std::max(durationSeconds, 0.01f) * static_cast<float>(sampleRate));

            std::vector<float> pcm(static_cast<size_t>(frameCount), 0.0f);
            for (uint64_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
            {
                const float t = static_cast<float>(frameIndex) / static_cast<float>(sampleRate);
                pcm[static_cast<size_t>(frameIndex)] = 0.25f * std::sin(2.0f * 3.14159265f * frequencyHz * t);
            }

            return AudioClip::CreateFromPcm(std::move(pcm), sampleRate, 1);
        }

        bool RunPlay2DAndStopTest(MockAudioBackend& backend)
        {
            const std::shared_ptr<AudioClip> clip = CreateTestToneClip();
            const AudioPlayResult playResult = AudioSystem::Get().Play2D(clip, EAudioBusId::SFX, 0.8f);
            if (!playResult.bSuccess)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: Play2D failed.");
                return false;
            }

            if (backend.GetCreateVoiceCount() == 0)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: backend did not create a voice.");
                return false;
            }

            const BackendVoiceHandle backendHandle{0};
            const MockAudioBackend::VoiceRecord* voice = backend.GetVoiceRecord(backendHandle);
            if (voice == nullptr || !voice->bPlaying)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: voice is not playing.");
                return false;
            }

            if (!AudioSystem::Get().StopVoice(playResult.Voice))
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: StopVoice failed.");
                return false;
            }

            if (backend.GetDestroyVoiceCount() == 0)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: backend did not destroy voice.");
                return false;
            }

            return true;
        }

        bool RunBusMuteTest(MockAudioBackend& backend)
        {
            const std::shared_ptr<AudioClip> clip = CreateTestToneClip(220.0f);
            AudioSystem::Get().GetMixer().SetBusMuted(EAudioBusId::SFX, true);
            const AudioPlayResult playResult = AudioSystem::Get().Play2D(clip);
            if (!playResult.bSuccess)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: bus mute play failed.");
                return false;
            }

            const MockAudioBackend::VoiceRecord* voice = backend.GetVoiceRecord(BackendVoiceHandle{0});
            if (voice == nullptr || voice->Volume != 0.0f)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: muted bus did not zero effective gain.");
                return false;
            }

            AudioSystem::Get().StopVoice(playResult.Voice);
            AudioSystem::Get().GetMixer().SetBusMuted(EAudioBusId::SFX, false);
            return true;
        }

        bool RunComponentAndSceneLifecycleTest()
        {
            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("audio-smoke");
            if (!scene)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: failed to create scene.");
                return false;
            }

            const std::shared_ptr<GameObject> emitterObject = scene->CreateGameObject();
            const std::shared_ptr<AudioComponent> audioComponent = emitterObject->AddComponent<AudioComponent>();
            audioComponent->SetClip(CreateTestToneClip(330.0f));
            audioComponent->Play();

            if (!audioComponent->IsPlaying())
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: AudioComponent did not start playing.");
                return false;
            }

            audioComponent->Stop();
            if (audioComponent->IsPlaying())
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: AudioComponent did not stop.");
                return false;
            }

            const std::shared_ptr<GameObject> listenerObject = scene->CreateGameObject();
            listenerObject->AddComponent<AudioListenerComponent>();

            SceneManager::Get().UnloadActiveScene();
            if (AudioSystem::Get().GetActiveVoiceCount() != 0)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: voices remained after scene unload.");
                return false;
            }

            return true;
        }

        bool RunPlay3DTest(MockAudioBackend& backend)
        {
            AudioSpatialSettings spatial;
            spatial.bSpatialized = true;
            spatial.MinDistance = 1.0f;
            spatial.MaxDistance = 50.0f;

            const AudioPlayResult playResult = AudioSystem::Get().Play3D(
                CreateTestToneClip(180.0f),
                Vector3(10.0f, 0.0f, 0.0f),
                spatial);
            if (!playResult.bSuccess)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: Play3D failed.");
                return false;
            }

            const MockAudioBackend::VoiceRecord* voice = backend.GetVoiceRecord(BackendVoiceHandle{0});
            if (voice == nullptr || !voice->bSpatializationEnabled)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: 3D voice spatialization was not enabled.");
                return false;
            }

            if (voice->Position.x != 10.0f)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: 3D voice position was not set.");
                return false;
            }

            AudioSystem::Get().StopVoice(playResult.Voice);
            return true;
        }

        bool RunPIEPlaybackGatingTest(MockAudioBackend& backend)
        {
            const std::shared_ptr<Scene> editorScene = SceneManager::Get().CreateNewScene("editor-audio-pie");
            if (!editorScene)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: failed to create editor scene.");
                return false;
            }

            editorScene->SetSceneType(ESceneType::Editor);

            const std::shared_ptr<GameObject> editorEmitterObject = editorScene->CreateGameObject();
            const std::shared_ptr<AudioComponent> editorAudio = editorEmitterObject->AddComponent<AudioComponent>();
            editorAudio->SetClip(CreateTestToneClip(260.0f));
            editorAudio->Play();

            if (!editorAudio->IsPlaying())
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: editor audio did not start before PIE.");
                return false;
            }

            const uint32_t voicesBeforePie = backend.GetCreateVoiceCount();

            const std::shared_ptr<Scene> pieScene = SceneManager::Get().CreateNewScene("pie-audio");
            if (!pieScene)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: failed to create PIE scene.");
                return false;
            }

            pieScene->SetSceneType(ESceneType::PIE);
            SceneManager::Get().SetPIEPlayActive(true);
            AudioSystem::Get().OnBeginPIE(pieScene.get());

            if (editorAudio->IsPlaying())
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: editor audio still playing after OnBeginPIE.");
                return false;
            }

            editorAudio->Play();
            if (editorAudio->IsPlaying())
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: editor audio started during PIE.");
                return false;
            }

            const std::shared_ptr<GameObject> pieEmitterObject = pieScene->CreateGameObject();
            const std::shared_ptr<AudioComponent> pieAudio = pieEmitterObject->AddComponent<AudioComponent>();
            pieAudio->SetClip(CreateTestToneClip(520.0f));
            pieAudio->Play();

            if (!pieAudio->IsPlaying())
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: PIE audio did not start during PIE.");
                return false;
            }

            if (backend.GetCreateVoiceCount() <= voicesBeforePie)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: PIE audio did not create a new voice.");
                return false;
            }

            SceneManager::Get().SetPIEPlayActive(false);
            AudioSystem::Get().OnEndPIE(pieScene.get());

            editorAudio->Play();
            if (!editorAudio->IsPlaying())
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: editor audio did not resume after PIE.");
                return false;
            }

            pieAudio->Stop();
            editorAudio->Stop();
            return true;
        }

        // A backend is handed the PCM the device will read, and it keeps that pointer for as long as
        // the voice lives. The clip that owns the buffer therefore has to outlive the voice, not the
        // caller that started playback: these probes model that by dropping the external handle.
        struct ClipBorrowProbe
        {
            std::weak_ptr<AudioClip> Watch{};
            const float* Pcm{nullptr};
            size_t SampleCount{0};
            float FirstSample{0.0f};
            float LastSample{0.0f};

            void Capture(const std::shared_ptr<AudioClip>& clip)
            {
                Watch = clip;
                const std::vector<float>& pcm = clip->GetPcmData();
                Pcm = pcm.data();
                SampleCount = pcm.size();
                FirstSample = pcm.front();
                LastSample = pcm.back();
            }

            bool StillAlive() const
            {
                return Watch.use_count() >= 1 && Watch.lock() != nullptr;
            }
        };

        bool RunClipOutlivesExternalOwnerTest(MockAudioBackend& backend)
        {
            std::shared_ptr<AudioClip> clip = CreateTestToneClip(440.0f, 0.05f);
            ClipBorrowProbe probe;
            probe.Capture(clip);

            const AudioPlayResult playResult = AudioSystem::Get().Play2D(clip, EAudioBusId::SFX, 1.0f);
            if (!playResult.bSuccess)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: Play2D failed.");
                return false;
            }

            const BackendVoiceHandle backendHandle{0};
            const MockAudioBackend::VoiceRecord* voice = backend.GetVoiceRecord(backendHandle);
            if (voice == nullptr)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: backend created no voice.");
                return false;
            }

            if (backend.GetBorrowedClipAddress(backendHandle) != clip.get()
                || backend.GetBorrowedPcm(backendHandle) != probe.Pcm)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: backend voice does not borrow the clip pcm.");
                return false;
            }

            clip.reset();

            if (!probe.StillAlive())
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: clip died while the voice was still playing.");
                return false;
            }

            const std::shared_ptr<AudioClip> pinned = probe.Watch.lock();
            if (pinned->GetPcmData().data() != probe.Pcm)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: live clip no longer owns the borrowed pcm.");
                return false;
            }

            float firstSample = 0.0f;
            float lastSample = 0.0f;
            if (!backend.ReadBorrowedSamples(backendHandle, firstSample, lastSample))
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: device-thread read was refused while playing.");
                return false;
            }

            if (firstSample != probe.FirstSample || lastSample != probe.LastSample)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: device-thread read returned unexpected samples.");
                return false;
            }

            AudioSystem::Get().StopVoice(playResult.Voice);
            return true;
        }

        bool RunStopReleasesClipTest(MockAudioBackend& backend)
        {
            std::shared_ptr<AudioClip> clip = CreateTestToneClip(220.0f, 0.05f);
            ClipBorrowProbe probe;
            probe.Capture(clip);

            const long externalOwners = probe.Watch.use_count();

            const AudioPlayResult playResult = AudioSystem::Get().Play2D(clip);
            if (!playResult.bSuccess)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: Play2D failed.");
                return false;
            }

            if (probe.Watch.use_count() <= externalOwners)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: playing voice holds no reference to the clip.");
                return false;
            }

            if (backend.GetBorrowedClipAddress(BackendVoiceHandle{0}) != clip.get())
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: backend voice did not borrow the playing clip.");
                return false;
            }

            if (!AudioSystem::Get().StopVoice(playResult.Voice))
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: StopVoice failed.");
                return false;
            }

            if (probe.Watch.use_count() != externalOwners)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: stop did not release the voice's clip reference.");
                return false;
            }

            if (backend.GetBorrowedClipAddress(BackendVoiceHandle{0}) != nullptr)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: stopped voice still borrows the clip.");
                return false;
            }

            float firstSample = 0.0f;
            float lastSample = 0.0f;
            if (backend.ReadBorrowedSamples(BackendVoiceHandle{0}, firstSample, lastSample))
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: device-thread read was accepted after stop.");
                return false;
            }

            return true;
        }

        bool RunNaturalEndReleasesClipTest(MockAudioBackend& backend)
        {
            std::shared_ptr<AudioClip> clip = CreateTestToneClip(330.0f, 0.05f);
            ClipBorrowProbe probe;
            probe.Capture(clip);

            const long externalOwners = probe.Watch.use_count();

            const AudioPlayResult playResult = AudioSystem::Get().Play2D(clip);
            if (!playResult.bSuccess)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: Play2D failed.");
                return false;
            }

            if (probe.Watch.use_count() <= externalOwners)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: playing voice holds no reference to the clip.");
                return false;
            }

            // The device reaches the end of the buffer on its own; the frame tick is what notices.
            backend.SimulatePlaybackEnd(BackendVoiceHandle{0});
            AudioSystem::Get().Tick(0.016f);

            if (AudioSystem::Get().GetActiveVoiceCount() != 0)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: finished voice stayed active.");
                return false;
            }

            if (AudioSystem::Get().FindVoice(playResult.Voice) != nullptr)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: finished voice was not freed.");
                return false;
            }

            if (probe.Watch.use_count() != externalOwners)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: natural end did not release the clip.");
                return false;
            }

            return true;
        }

        bool RunSharedClipReleasedByLastVoiceTest(MockAudioBackend& backend)
        {
            std::shared_ptr<AudioClip> clip = CreateTestToneClip(550.0f, 0.05f);
            ClipBorrowProbe probe;
            probe.Capture(clip);

            const long externalOwners = probe.Watch.use_count();

            const AudioPlayResult firstPlay = AudioSystem::Get().Play2D(clip);
            const AudioPlayResult secondPlay = AudioSystem::Get().Play2D(clip);
            if (!firstPlay.bSuccess || !secondPlay.bSuccess)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: playing one clip on two voices failed.");
                return false;
            }

            AudioSystem::Get().StopVoice(firstPlay.Voice);
            if (probe.Watch.use_count() <= externalOwners)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: clip was released while the second voice still plays.");
                return false;
            }

            AudioSystem::Get().StopVoice(secondPlay.Voice);
            if (probe.Watch.use_count() != externalOwners)
            {
                ME_LOG(LogTest, Error, "AudioSmokeTest: clip survived the last voice.");
                return false;
            }

            return true;
        }
    }

    bool RunAudioSmokeTests()
    {
        AudioSmokeTestScope scope;
        MockAudioBackend* backend = scope.GetBackend();
        if (backend == nullptr)
        {
            ME_LOG(LogTest, Error, "AudioSmokeTest: mock backend missing.");
            return false;
        }

        return RunPlay2DAndStopTest(*backend) && RunBusMuteTest(*backend) && RunPlay3DTest(*backend)
            && RunComponentAndSceneLifecycleTest() && RunPIEPlaybackGatingTest(*backend);
    }
}

#include "doctest.h"

#include "EngineTestFixture.h"

TEST_CASE("audio-smoke: play2d, bus mute, 3d, component lifecycle [smoke][full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunAudioSmokeTests());
}

TEST_CASE("audio-smoke: clip outlives the caller that started playback [smoke][full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::AudioSmokeTestScope scope;
    minEngine::MockAudioBackend* backend = scope.GetBackend();
    REQUIRE(backend != nullptr);
    CHECK(minEngine::RunClipOutlivesExternalOwnerTest(*backend));
}

TEST_CASE("audio-smoke: stop releases the voice's clip reference [smoke][full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::AudioSmokeTestScope scope;
    minEngine::MockAudioBackend* backend = scope.GetBackend();
    REQUIRE(backend != nullptr);
    CHECK(minEngine::RunStopReleasesClipTest(*backend));
}

TEST_CASE("audio-smoke: natural end releases the clip [smoke][full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::AudioSmokeTestScope scope;
    minEngine::MockAudioBackend* backend = scope.GetBackend();
    REQUIRE(backend != nullptr);
    CHECK(minEngine::RunNaturalEndReleasesClipTest(*backend));
}

TEST_CASE("audio-smoke: playback refuses a clip it cannot own [smoke][full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::AudioSmokeTestScope scope;
    REQUIRE(scope.GetBackend() != nullptr);

    CHECK_FALSE(minEngine::AudioSystem::Get().Play2D(nullptr).bSuccess);

    // A lease is the ownership a backend needs before it can read samples, so an unleased one hands
    // out nothing at all.
    minEngine::AudioClipLease noClip;
    CHECK_FALSE(noClip.IsValid());
    CHECK(noClip.GetClip() == nullptr);
}

TEST_CASE("audio-smoke: a shared clip is released by its last voice [smoke][full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::AudioSmokeTestScope scope;
    minEngine::MockAudioBackend* backend = scope.GetBackend();
    REQUIRE(backend != nullptr);
    CHECK(minEngine::RunSharedClipReleasedByLastVoiceTest(*backend));
}

