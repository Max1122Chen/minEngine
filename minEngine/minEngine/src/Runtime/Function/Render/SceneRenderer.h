#pragma once

#include "Core.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"

#include <string>

namespace minEngine
{
    /**
     * Thin scene renderer API for RenderSystem.
     * Forward/Deferred implementations plug in here; no deferred-specific hooks yet.
     */
    class SceneRenderer
    {
    public:
        virtual ~SceneRenderer() = default;

        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;
        virtual void Execute(const SceneDrawDesc& desc) = 0;
        virtual void SetPresentPassEnabled(bool enabled) = 0;
        virtual void LoadEngineRenderingAssets(const std::string& engineDefaultAssetsRoot) = 0;
    };
}
