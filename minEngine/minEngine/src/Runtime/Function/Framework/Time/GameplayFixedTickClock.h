#pragma once

#include "Core.h"

#include <cstdint>

namespace minEngine
{
    /** Engine-owned gameplay FixedTick accumulator; independent from PhysicsWorld fixed step. */
    class GameplayFixedTickClock
    {
    public:
        bool IsEnabled() const { return GetIntervalSeconds() > 0.0f; }
        bool IsPaused() const { return m_bPaused; }

        /** Effective interval including a pending change not yet flushed. */
        float GetIntervalSeconds() const;
        void SetIntervalSeconds(float seconds);

        float GetTicksPerSecond() const;
        void SetTicksPerSecond(float ticksPerSecond);

        float GetTimeScale() const { return m_TimeScale; }
        void SetTimeScale(float scale);

        int GetMaxStepsPerFrame() const { return m_MaxStepsPerFrame; }
        void SetMaxStepsPerFrame(int maxSteps);

        uint64_t GetTickIndex() const { return m_TickIndex; }

        void SetPaused(bool paused) { m_bPaused = paused; }

        float GetAccumulator() const { return m_Accumulator; }

        /**
         * Consume frameDt into the accumulator and return how many fixed steps to run.
         * Applies pending interval changes before accumulating.
         */
        int BeginFrameAndConsumeSteps(float frameDt);

        /** Call once after each completed fixed step dispatch. */
        void NotifyStepCompleted();

        /** Apply any interval change requested during a FixedTick callback. */
        void FlushPendingInterval();

    private:
        float m_IntervalSeconds = 0.0f;
        float m_PendingIntervalSeconds = -1.0f;
        float m_TimeScale = 1.0f;
        float m_Accumulator = 0.0f;
        int m_MaxStepsPerFrame = 8;
        uint64_t m_TickIndex = 0;
        bool m_bPaused = false;
        bool m_bWarnedMaxStepsThisFrame = false;
    };
}
