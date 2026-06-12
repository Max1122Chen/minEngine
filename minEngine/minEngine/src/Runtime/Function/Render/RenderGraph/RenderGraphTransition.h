#pragma once

#include "Core.h"
#include "Render/RenderGraph/RDGTexture.h"

namespace minEngine
{
    class RHICommandList;
    class RenderGraphFrameResources;

    void AddTransition(
        RHICommandList& cmdList,
        const char* textureName,
        RenderGraphFrameResources& frameResources,
        RDGTextureUsage before,
        RDGTextureUsage after);
}
