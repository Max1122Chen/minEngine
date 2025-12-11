#include "Component.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Framework/World/WorldManager.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    Component::Component()
    {
    }

    void Component::SetOwner(GameObject *inOwner)
    {
        m_Owner = std::shared_ptr<GameObject>(inOwner);
    }

    void Component::MarkForNeededEndOfFrameUpdate()
    {
        RuntimeGlobalContext::GetRuntimeGlobalContext().m_WorldManager->MarkComponentForNeededEndOfFrameUpdate(this);
    }
}
