#include "Component.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Framework/World/WorldManager.h"

namespace minEngine
{
    void Component::MarkForNeededEndOfFrameUpdate()
    {
        RuntimeGlobalContext::GetInstance().m_WorldManager->MarkComponentForNeededEndOfFrameUpdate(this);
    }
}
