#pragma once

#include "Runtime/Core/Threading/RenderCommandPipe.h"

namespace minEngine::Testing
{
    template<>
    class TestAccess<RenderCommandPipe>
    {
    public:
        /**
         * Writes the two published counters directly, without going through PublishGameFrame.
         *
         * A test needs the pair a producer samples when it lands between the publisher's two
         * stores: the drain target already moved on while the tag still names the frame before it.
         * PublishGameFrame cannot produce that state from the outside - it publishes both counters
         * as one step - so the pair is written here instead.
         */
        static void StorePublishedFrames(
            RenderCommandPipe& pipe,
            uint64_t completedFrame,
            uint64_t currentFrame)
        {
            pipe.m_CompletedGameFrame.store(completedFrame, std::memory_order_relaxed);
            pipe.m_CurrentGameFrame.store(currentFrame, std::memory_order_relaxed);
        }
    };
}
