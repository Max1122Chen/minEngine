#pragma once

#include "Core.h"
#include "Runtime/Function/Debug/DebugDrawTypes.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

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
        EnableSkyBox = 1u << 3,
        EnableDebugDraw = 1u << 4,

        Default = EnableShadows | EnablePostProcess | PresentToBackBuffer | EnableSkyBox,
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
        /** stable view identity from SceneViewport; 0 = invalid. */
        uint64_t ViewId = 0;
        SceneDrawFlags Flags = SceneDrawFlags::Default;

        /** Clear color applied to the scene color target (RGBA). Default slate-blue, kept for back-compat. */
        float ClearColor[4] = {0.18f, 0.32f, 0.48f, 1.0f};
        /** Clear depth applied to the scene depth target. */
        float DepthClear = 1.0f;

        /**
         * Debug geometry the world side built for this view's frame. The drawing side copies the
         * commands out of it and never reaches the scene they were read from, so a frame can be
         * drawn while the world keeps changing. Empty = nothing to draw.
         */
        DebugDrawSnapshotRef DebugSnapshot;

        /**
         * Optional caller-owned SceneColor. When set, ForwardRenderer registers it as an RDG
         * external for this draw (zero-copy render-into). Empty = graph allocates SceneColor.
         */
        RHITextureRef ExternalSceneColor;
    };
}
