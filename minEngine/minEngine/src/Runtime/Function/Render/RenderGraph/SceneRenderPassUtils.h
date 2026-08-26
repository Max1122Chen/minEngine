#pragma once

#include "Render/RHI/RHIRenderPass.h"

namespace minEngine
{
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
