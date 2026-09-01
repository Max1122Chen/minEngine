#include "RHIBackend.h"

namespace minEngine
{
    RHIBackendType RHIBackendSelection::s_Backend = RHIBackendType::OpenGL;

    void RHIBackendSelection::Set(RHIBackendType backend)
    {
        s_Backend = backend;
    }

    RHIBackendType RHIBackendSelection::Get()
    {
        return s_Backend;
    }

    bool RHIBackendSelection::IsVulkan()
    {
        return s_Backend == RHIBackendType::Vulkan;
    }

    bool RHIBackendSelection::IsOpenGL()
    {
        return s_Backend == RHIBackendType::OpenGL;
    }
}
