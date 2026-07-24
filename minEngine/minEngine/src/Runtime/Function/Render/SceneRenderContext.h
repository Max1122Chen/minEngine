#pragma once

#include "Core.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

#include <unordered_map>
#include <vector>

namespace minEngine
{
    class RenderScene;
    class RenderCamera;
    class SpotLightSceneProxy;
    class PointLightSceneProxy;

    /** Per-Execute transient state (queues, shadow build results). Not stored on ForwardRenderer. */
    struct SceneRenderContext
    {
        RenderScene* Scene = nullptr;
        RenderCamera* Camera = nullptr;

        std::vector<MeshDrawCommand> OpaqueQueue;
        std::vector<MeshDrawCommand> TranslucentQueue;

        std::vector<ShadowRequest> ShadowRequests;
        std::vector<ShadowDrawCommand> ShadowDrawCommands;

        ShadowResourceHandle DirectionalShadowHandle;
        std::vector<ShadowResourceHandle> SpotShadowHandles;
        std::vector<ShadowResourceHandle> PointShadowHandles;
        std::unordered_map<const SpotLightSceneProxy*, ShadowResourceHandle> SpotShadowHandleMap;
        std::unordered_map<const PointLightSceneProxy*, ShadowResourceHandle> PointShadowHandleMap;

        void ResetFrame()
        {
            Scene = nullptr;
            Camera = nullptr;
            OpaqueQueue.clear();
            TranslucentQueue.clear();
            ShadowRequests.clear();
            ShadowDrawCommands.clear();
            DirectionalShadowHandle = ShadowResourceHandle{};
            SpotShadowHandles.clear();
            PointShadowHandles.clear();
            SpotShadowHandleMap.clear();
            PointShadowHandleMap.clear();
        }
    };
}
