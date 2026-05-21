#pragma once

#include "Core.h"

namespace minEngine
{
    class RenderScene;
    class RenderCamera;
    class SceneRenderTarget;

    enum class SceneDrawFlags : uint32_t
    {
        None = 0,
        EnableShadows = 1u << 0,
        EnablePostProcess = 1u << 1,
        PresentToBackBuffer = 1u << 2,

        Default = EnableShadows | EnablePostProcess | PresentToBackBuffer,
    };

    inline SceneDrawFlags operator|(SceneDrawFlags lhs, SceneDrawFlags rhs)
    {
        return static_cast<SceneDrawFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    inline bool HasSceneDrawFlag(SceneDrawFlags flags, SceneDrawFlags test)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
    }

    /** Per-view submission: scene content + camera + output RT. */
    struct SceneDrawDesc
    {
        RenderScene* Scene = nullptr;
        RenderCamera* Camera = nullptr;
        SceneRenderTarget* RenderTarget = nullptr;
        SceneDrawFlags Flags = SceneDrawFlags::Default;
    };
}
