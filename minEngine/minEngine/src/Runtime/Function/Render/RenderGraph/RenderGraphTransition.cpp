#include "Render/RenderGraph/RenderGraphTransition.h"

#include "Render/RenderGraph/RenderGraphFrameResources.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIResourceTransition.h"

namespace minEngine
{
    void AddTransition(
        RHICommandList& cmdList,
        const char* textureName,
        RenderGraphFrameResources& frameResources,
        RDGTextureUsage before,
        RDGTextureUsage after)
    {
        (void)before;

        RHITexture* texture = frameResources.GetRHI(textureName);
        if (texture != nullptr)
        {
            RHITextureTransitionInfo transition;
            transition.Texture = texture;
            cmdList.Transition(transition);
        }

        frameResources.SetLastKnownUsage(textureName, after);
    }
}
