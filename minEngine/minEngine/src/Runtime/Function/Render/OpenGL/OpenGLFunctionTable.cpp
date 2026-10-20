#include "OpenGLFunctionTable.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include <atomic>

namespace minEngine
{
    bool EnsureOpenGLFunctionTableLoaded()
    {
        static std::atomic<bool> s_Loaded{false};
        if (s_Loaded.load(std::memory_order_acquire))
        {
            return true;
        }

        // Nothing to resolve against: the caller's GL work is invalid with or without this guard,
        // so return instead of calling gladLoadGLLoader(NULL) and reporting a load failure.
        if (glfwGetCurrentContext() == nullptr)
        {
            return false;
        }

        if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0)
        {
            return false;
        }

        s_Loaded.store(true, std::memory_order_release);
        return true;
    }
}
