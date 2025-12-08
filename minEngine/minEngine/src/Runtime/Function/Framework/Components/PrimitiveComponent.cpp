#include "PrimitiveComponent.h"
#include "Runtime/Function/Render/PrimitiveSceneProxy.h"

namespace minEngine
{
    void PrimitiveComponent::MarkRenderStateDirty()
    {
        m_bRenderStateDirty = true;
        MarkForNeededEndOfFrameUpdate();
    }

    void PrimitiveComponent::SetTransform(const Transform &inTransform)
    {
        if(m_Transform == inTransform)
        {
            return;
        }
        m_Transform = inTransform;
        MarkRenderStateDirty();
    }
} 




