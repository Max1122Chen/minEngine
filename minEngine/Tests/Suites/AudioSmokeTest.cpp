#include "AudioSmokeTest.h"

#include "MockAudioBackend.h"

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
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            SceneManager::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            AudioSystem::SetInstance(&m_AudioSystem);
            m_AudioSystem.InitializeWithBackend(std::make_unique<MockAudioBackend>());
        }

        ~AudioSmokeTestScope()
        {
            m_SceneManager.Shutdown();
            SceneManager::SetInstance(nullptr);

            m_AudioSystem.Shutdown();
            AudioSystem::SetInstance(nullptr);

            m_ObjectManager.Shutdown();
            ObjectManager::SetInstance(nullptr);
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
                ME_CORE_ERROR("AudioSmokeTest: Play2D failed.");
                return false;
            }

            if (backend.GetCreateVoiceCount() == 0)
            {
                ME_CORE_ERROR("AudioSmokeTest: backend did not create a voice.");
                return false;
            }

            const BackendVoiceHandle backendHandle{0};
            const MockAudioBackend::VoiceRecord* voice = backend.GetVoiceRecord(backendHandle);
            if (voice == nullptr || !voice->bPlaying)
            {
                ME_CORE_ERROR("AudioSmokeTest: voice is not playing.");
                return false;
            }

            if (!AudioSystem::Get().StopVoice(playResult.Voice))
            {
                ME_CORE_ERROR("AudioSmokeTest: StopVoice failed.");
                return false;
            }

            if (backend.GetDestroyVoiceCount() == 0)
            {
                ME_CORE_ERROR("AudioSmokeTest: backend did not destroy voice.");
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
                ME_CORE_ERROR("AudioSmokeTest: bus mute play failed.");
                return false;
            }

            const MockAudioBackend::VoiceRecord* voice = backend.GetVoiceRecord(BackendVoiceHandle{0});
            if (voice == nullptr || voice->Volume != 0.0f)
            {
                ME_CORE_ERROR("AudioSmokeTest: muted bus did not zero effective gain.");
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
                ME_CORE_ERROR("AudioSmokeTest: failed to create scene.");
                return false;
            }

            const std::shared_ptr<GameObject> emitterObject = scene->CreateGameObject();
            const std::shared_ptr<AudioComponent> audioComponent = emitterObject->AddComponent<AudioComponent>();
            audioComponent->SetClip(CreateTestToneClip(330.0f));
            audioComponent->Play();

            if (!audioComponent->IsPlaying())
            {
                ME_CORE_ERROR("AudioSmokeTest: AudioComponent did not start playing.");
                return false;
            }

            audioComponent->Stop();
            if (audioComponent->IsPlaying())
            {
                ME_CORE_ERROR("AudioSmokeTest: AudioComponent did not stop.");
                return false;
            }

            const std::shared_ptr<GameObject> listenerObject = scene->CreateGameObject();
            listenerObject->AddComponent<AudioListenerComponent>();

            SceneManager::Get().UnloadActiveScene();
            if (AudioSystem::Get().GetActiveVoiceCount() != 0)
            {
                ME_CORE_ERROR("AudioSmokeTest: voices remained after scene unload.");
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
                ME_CORE_ERROR("AudioSmokeTest: Play3D failed.");
                return false;
            }

            const MockAudioBackend::VoiceRecord* voice = backend.GetVoiceRecord(BackendVoiceHandle{0});
            if (voice == nullptr || !voice->bSpatializationEnabled)
            {
                ME_CORE_ERROR("AudioSmokeTest: 3D voice spatialization was not enabled.");
                return false;
            }

            if (voice->Position.x != 10.0f)
            {
                ME_CORE_ERROR("AudioSmokeTest: 3D voice position was not set.");
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
                ME_CORE_ERROR("AudioSmokeTest: failed to create editor scene.");
                return false;
            }

            editorScene->SetSceneType(ESceneType::Editor);

            const std::shared_ptr<GameObject> editorEmitterObject = editorScene->CreateGameObject();
            const std::shared_ptr<AudioComponent> editorAudio = editorEmitterObject->AddComponent<AudioComponent>();
            editorAudio->SetClip(CreateTestToneClip(260.0f));
            editorAudio->Play();

            if (!editorAudio->IsPlaying())
            {
                ME_CORE_ERROR("AudioSmokeTest: editor audio did not start before PIE.");
                return false;
            }

            const uint32_t voicesBeforePie = backend.GetCreateVoiceCount();

            const std::shared_ptr<Scene> pieScene = SceneManager::Get().CreateNewScene("pie-audio");
            if (!pieScene)
            {
                ME_CORE_ERROR("AudioSmokeTest: failed to create PIE scene.");
                return false;
            }

            pieScene->SetSceneType(ESceneType::PIE);
            SceneManager::Get().SetPIEPlayActive(true);
            AudioSystem::Get().OnBeginPIE(pieScene.get());

            if (editorAudio->IsPlaying())
            {
                ME_CORE_ERROR("AudioSmokeTest: editor audio still playing after OnBeginPIE.");
                return false;
            }

            editorAudio->Play();
            if (editorAudio->IsPlaying())
            {
                ME_CORE_ERROR("AudioSmokeTest: editor audio started during PIE.");
                return false;
            }

            const std::shared_ptr<GameObject> pieEmitterObject = pieScene->CreateGameObject();
            const std::shared_ptr<AudioComponent> pieAudio = pieEmitterObject->AddComponent<AudioComponent>();
            pieAudio->SetClip(CreateTestToneClip(520.0f));
            pieAudio->Play();

            if (!pieAudio->IsPlaying())
            {
                ME_CORE_ERROR("AudioSmokeTest: PIE audio did not start during PIE.");
                return false;
            }

            if (backend.GetCreateVoiceCount() <= voicesBeforePie)
            {
                ME_CORE_ERROR("AudioSmokeTest: PIE audio did not create a new voice.");
                return false;
            }

            SceneManager::Get().SetPIEPlayActive(false);
            AudioSystem::Get().OnEndPIE(pieScene.get());

            editorAudio->Play();
            if (!editorAudio->IsPlaying())
            {
                ME_CORE_ERROR("AudioSmokeTest: editor audio did not resume after PIE.");
                return false;
            }

            pieAudio->Stop();
            editorAudio->Stop();
            return true;
        }
    }

    bool RunAudioSmokeTests()
    {
        AudioSmokeTestScope scope;
        MockAudioBackend* backend = scope.GetBackend();
        if (backend == nullptr)
        {
            ME_CORE_ERROR("AudioSmokeTest: mock backend missing.");
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
