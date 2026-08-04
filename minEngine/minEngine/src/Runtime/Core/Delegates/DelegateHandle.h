#pragma once

#include <atomic>
#include <cstdint>

namespace minEngine
{
    /// Opaque id returned by MulticastDelegate::Add*; used for Remove.
    class DelegateHandle
    {
    public:
        DelegateHandle() = default;

        bool IsValid() const { return m_Id != 0; }

        bool operator==(const DelegateHandle& other) const { return m_Id == other.m_Id; }
        bool operator!=(const DelegateHandle& other) const { return m_Id != other.m_Id; }

        static DelegateHandle Create()
        {
            DelegateHandle handle;
            handle.m_Id = NextId().fetch_add(1, std::memory_order_relaxed);
            return handle;
        }

        static DelegateHandle Invalid() { return DelegateHandle{}; }

    private:
        static std::atomic<uint64_t>& NextId()
        {
            // Ids start at 1 so that 0 remains Invalid.
            static std::atomic<uint64_t> s_NextId{1};
            return s_NextId;
        }

        uint64_t m_Id = 0;
    };
}
