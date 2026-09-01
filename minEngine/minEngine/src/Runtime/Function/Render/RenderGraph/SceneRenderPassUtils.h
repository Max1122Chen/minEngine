#pragma once

#include "Render/RHI/RHIRenderPass.h"
#include "Render/RenderGraph/RDGTypes.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

#include <string>

namespace minEngine
{
    /** Declare shadow atlas read edges for lit scene passes (RND-F12-S01 / Granite generic_texture_input). */
    inline void AddSceneLitShadowTextureInputs(RenderPass& self)
    {
        self.AddTextureInput(kRDGDirShadowAtlas);
        for (int spotIndex = 0; spotIndex < MAX_SPOT_SHADOW_MAPS; ++spotIndex)
        {
            self.AddTextureInput("SpotShadow." + std::to_string(spotIndex));
        }
        for (int pointIndex = 0; pointIndex < MAX_POINT_SHADOW_MAPS; ++pointIndex)
        {
            self.AddTextureInput("PointShadow." + std::to_string(pointIndex));
        }
    }

    /** Shared BeginRenderPass info for scene color+depth passes (Sky / Opaque / Translucent). */
    inline RHIRenderPassInfo MakeSceneRenderPassInfo(RHITexture* color, RHITexture* depth, bool clearTargets)
    {
        RHIRenderPassInfo info(
            color,
            clearTargets ? RHIRenderTargetActions::ClearStore : RHIRenderTargetActions::LoadStore,
            depth,
            clearTargets ? RHIDepthStencilTargetActions::ClearDepthStencilStoreDepthStencil
                         : RHIDepthStencilTargetActions::LoadDepthStencilStoreDepthStencil);
        if (clearTargets)
        {
            // Default scene clear when Sky is off / BasePass owns clear.
            info.ClearValue.Color[0] = 0.18f;
            info.ClearValue.Color[1] = 0.32f;
            info.ClearValue.Color[2] = 0.48f;
            info.ClearValue.Color[3] = 1.0f;
            info.ClearValue.Depth = 1.0f;
        }
        return info;
    }
}
