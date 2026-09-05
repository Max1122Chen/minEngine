#pragma once

#include "Core.h"

#include <memory>

namespace minEngine
{
    class AnimationClip;
    struct Pose;

    enum class AnimationPlayState : uint8_t
    {
        Stopped = 0,
        Playing = 1,
        Paused = 2,
    };

    // Composed into SkeletalMeshComponent (F02). Never touches RHI / Assimp.
    class AnimationPlayer
    {
    public:
        void SetClip(const std::shared_ptr<AnimationClip>& clip);
        AnimationClip* GetClip() const { return m_Clip.get(); }

        void Play();
        void Pause();
        void Stop();

        void SetLooping(bool loop) { m_bLooping = loop; }
        bool IsLooping() const { return m_bLooping; }

        void SetSpeed(float speed) { m_Speed = speed; }
        float GetSpeed() const { return m_Speed; }

        void SetTime(float timeSeconds);
        float GetTime() const { return m_Time; }

        AnimationPlayState GetState() const { return m_State; }

        // Advances time when Playing and evaluates into outPose.
        void Update(float deltaSeconds, Pose& outPose);

    private:
        void WrapOrClampTime();

        std::shared_ptr<AnimationClip> m_Clip;
        float m_Time = 0.0f;
        float m_Speed = 1.0f;
        bool m_bLooping = true;
        // F02 test default: auto-play when a clip is assigned (Inspector has no Play UI yet).
        AnimationPlayState m_State = AnimationPlayState::Playing;
    };
}
