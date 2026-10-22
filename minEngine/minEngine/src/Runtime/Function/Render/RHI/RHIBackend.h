#pragma once

#include "Runtime/Core/EngineAPI.h"

#include <cstdint>

namespace minEngine
{
    enum class RHIBackendType : uint8_t
    {
        OpenGL = 0,
        Vulkan,
    };

    /** Process-wide backend chosen from CLI before Window/Render init. */
    class MINENGINE_API RHIBackendSelection
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
