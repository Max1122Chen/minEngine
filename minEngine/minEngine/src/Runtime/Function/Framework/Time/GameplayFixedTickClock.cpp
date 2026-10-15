#include "GameplayFixedTickClock.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <algorithm>
#include <cmath>

namespace minEngine
{
    float GameplayFixedTickClock::GetIntervalSeconds() const
    {
        if (m_PendingIntervalSeconds >= 0.0f)
        {
            return m_PendingIntervalSeconds;
        }
        return m_IntervalSeconds;
    }

    void GameplayFixedTickClock::SetIntervalSeconds(float seconds)
    {
        if (!std::isfinite(seconds))
        {
            ME_LOG(LogCore, Warn, "GameplayFixedTickClock: ignoring non-finite Interval.");
            return;
        }

        // Applied on BeginFrame / phase end (not mid-step).
        m_PendingIntervalSeconds = (seconds <= 0.0f) ? 0.0f : seconds;
    }

    float GameplayFixedTickClock::GetTicksPerSecond() const
    {
        if (m_IntervalSeconds <= 0.0f)
        {
            return 0.0f;
        }
        return 1.0f / m_IntervalSeconds;
    }

    void GameplayFixedTickClock::SetTicksPerSecond(float ticksPerSecond)
    {
        if (!std::isfinite(ticksPerSecond) || ticksPerSecond <= 0.0f)
        {
            SetIntervalSeconds(0.0f);
            return;
        }
        SetIntervalSeconds(1.0f / ticksPerSecond);
    }

    void GameplayFixedTickClock::SetTimeScale(float scale)
    {
        if (!std::isfinite(scale))
        {
            ME_LOG(LogCore, Warn, "GameplayFixedTickClock: ignoring non-finite TimeScale.");
            return;
        }
        m_TimeScale = std::max(0.0f, scale);
    }

    void GameplayFixedTickClock::SetMaxStepsPerFrame(int maxSteps)
    {
        m_MaxStepsPerFrame = std::max(1, maxSteps);
    }

    void GameplayFixedTickClock::FlushPendingInterval()
    {
        if (m_PendingIntervalSeconds < 0.0f)
        {
            return;
        }

        m_IntervalSeconds = m_PendingIntervalSeconds;
        m_PendingIntervalSeconds = -1.0f;
        if (m_IntervalSeconds <= 0.0f)
        {
            m_IntervalSeconds = 0.0f;
            m_Accumulator = 0.0f;
        }
    }

    int GameplayFixedTickClock::BeginFrameAndConsumeSteps(float frameDt)
    {
        FlushPendingInterval();
        m_bWarnedMaxStepsThisFrame = false;

        if (!IsEnabled() || m_bPaused || !std::isfinite(frameDt) || frameDt < 0.0f)
        {
            return 0;
        }

        m_Accumulator += frameDt * m_TimeScale;

        int steps = 0;
        while (m_Accumulator >= m_IntervalSeconds && steps < m_MaxStepsPerFrame)
        {
            m_Accumulator -= m_IntervalSeconds;
            ++steps;
        }

        if (m_Accumulator >= m_IntervalSeconds && !m_bWarnedMaxStepsThisFrame)
        {
            ME_LOG(LogCore, Warn,
                "GameplayFixedTickClock: hit MaxStepsPerFrame={} (accum remains {:.4f}).",
                m_MaxStepsPerFrame,
                m_Accumulator);
            m_bWarnedMaxStepsThisFrame = true;
        }

        return steps;
    }

    void GameplayFixedTickClock::NotifyStepCompleted()
    {
        ++m_TickIndex;
    }
}
