#include "PrimitiveComponent.h"
#include "Runtime/Function/Framework/World/WorldManager.h"
#include "Runtime/Function/Render/RenderScene.h"

namespace minEngine
{
    PrimitiveComponent::PrimitiveComponent()
    {
    }

    void PrimitiveComponent::DoEndOfFrameUpdate()
    {
        if(m_bRenderStateDirty)     // why do we need this check again? 
        {
            WorldManager::GetWorldManager().GetRenderScene()->UpdatePrimitive(this);
            m_bRenderStateDirty = false;
        }
    }

}
