#include "Runtime/Function/Render/RHI/RHI.h"

namespace minEngine
{
    /**
     * One serial counter for the whole process, defined here rather than in the header.
     *
     * The serial is a cache key (a program cache, a material cache), so two devices that were handed
     * the same number would serve each other's objects. A counter in the header would exist once per
     * module and every module's first device would be serial 1; the definition lives in the runtime
     * module so every module takes its number from the same sequence.
     */
    namespace
    {
        std::atomic<uint64_t> s_NextRHIInstanceSerial{ 0 };
    }

    namespace RHIRegistry
    {
        uint64_t NextInstanceSerial()
        {
            return s_NextRHIInstanceSerial.fetch_add(1, std::memory_order_relaxed) + 1;
        }

        uint64_t CurrentInstanceSerial()
        {
            return s_NextRHIInstanceSerial.load(std::memory_order_relaxed);
        }
    }
}
