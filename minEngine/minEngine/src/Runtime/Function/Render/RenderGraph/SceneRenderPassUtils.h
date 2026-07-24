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
            info.ClearValue.Color[0] = 0.1f;
            info.ClearValue.Color[1] = 0.1f;
            info.ClearValue.Color[2] = 0.1f;
            info.ClearValue.Color[3] = 1.0f;
            info.ClearValue.Depth = 1.0f;
        }
        return info;
    }
}
