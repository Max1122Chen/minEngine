#include "Runtime/Function/Render/RHI/RHIDeferredDestroy.h"

namespace minEngine
{
    /**
     * The registry a resource destructor posts through, defined once for the whole process.
     *
     * A resource released inside the runtime module and a host that installs a queue must be talking
     * about the same storage. A function-local static in the header would be instantiated once per
     * module instead, so these definitions live here and the header only declares them; the class
     * keeps its inline entry points and they all route through this one definition.
     */
    namespace RHIDeferredDestroyState
    {
        std::mutex& RegistryMutex()
        {
            static std::mutex s_Mutex;
            return s_Mutex;
        }

        std::weak_ptr<RHIDeferredDestroyQueue>& ActiveQueueSlot()
        {
            static std::weak_ptr<RHIDeferredDestroyQueue> s_Queue;
            return s_Queue;
        }

        std::atomic<std::uint64_t>& PostedWithoutQueueCounter()
        {
            static std::atomic<std::uint64_t> s_Count{ 0 };
            return s_Count;
        }
    }
}
