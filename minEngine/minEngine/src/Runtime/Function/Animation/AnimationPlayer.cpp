#include "Runtime/Function/Animation/AnimationPlayer.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Animation/AnimationClip.h"
#include "Runtime/Function/Animation/Pose.h"

#include <cmath>

namespace minEngine
{
    void AnimationPlayer::SetClip(const std::shared_ptr<AnimationClip>& clip)
    {
        m_Clip = clip;
        m_Time = 0.0f;
        // Keep Playing when a usable clip is assigned so Inspector assign is immediately testable.
        if (m_Clip != nullptr && m_Clip->GetDuration() > 0.0f && m_Clip->GetSkeleton() != nullptr)
        {
            m_State = AnimationPlayState::Playing;
        }
        else
        {
            m_State = AnimationPlayState::Stopped;
        }
    }

    void AnimationPlayer::Play()
    {
        if (m_Clip == nullptr || m_Clip->GetDuration() <= 0.0f)
        {
            ME_CORE_ERROR("AnimationPlayer::Play rejected: missing clip or non-positive duration.");
            return;
        }
        if (m_Clip->GetSkeleton() == nullptr)
        {
            ME_CORE_ERROR("AnimationPlayer::Play rejected: clip has no Skeleton.");
            return;
        }
        m_State = AnimationPlayState::Playing;
    }

    void AnimationPlayer::Pause()
    {
        if (m_State == AnimationPlayState::Playing)
        {
            m_State = AnimationPlayState::Paused;
        }
    }

    void AnimationPlayer::Stop()
    {
        m_Time = 0.0f;
        m_State = AnimationPlayState::Stopped;
    }

    void AnimationPlayer::SetTime(float timeSeconds)
    {
        m_Time = timeSeconds;
        WrapOrClampTime();
    }

    void AnimationPlayer::WrapOrClampTime()
    {
        if (m_Clip == nullptr)
        {
            m_Time = 0.0f;
            return;
        }

        const float duration = m_Clip->GetDuration();
        if (duration <= 0.0f)
        {
            m_Time = 0.0f;
            return;
        }

        if (m_bLooping)
        {
            m_Time = std::fmod(m_Time, duration);
            if (m_Time < 0.0f)
            {
                m_Time += duration;
            }
            return;
        }

        if (m_Time < 0.0f)
        {
            m_Time = 0.0f;
            if (m_State == AnimationPlayState::Playing)
            {
                m_State = AnimationPlayState::Paused;
            }
        }
        else if (m_Time >= duration)
        {
            m_Time = duration;
            if (m_State == AnimationPlayState::Playing)
            {
                m_State = AnimationPlayState::Paused;
            }
        }
    }

    void AnimationPlayer::Update(float deltaSeconds, Pose& outPose)
    {
        if (m_State != AnimationPlayState::Playing || m_Clip == nullptr)
        {
            return;
        }

        m_Time += deltaSeconds * m_Speed;
        WrapOrClampTime();
        m_Clip->Evaluate(m_Time, outPose);
    }
}
