#pragma once

#include <cstdint>

namespace minEngine
{
    enum class RHIBackendType : uint8_t
    {
        OpenGL = 0,
        Vulkan,
    };

    /** Process-wide backend chosen from CLI before Window/Render init (RND-F05-S03). */
    class RHIBackendSelection
    {
    public:
        static void Set(RHIBackendType backend);
        static RHIBackendType Get();
        static bool IsVulkan();
        static bool IsOpenGL();

    private:
        static RHIBackendType s_Backend;
    };
}
