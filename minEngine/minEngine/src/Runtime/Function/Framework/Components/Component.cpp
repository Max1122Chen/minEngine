#include "Component.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    Component::Component()
    {
    }

    void Component::SetOwner(GameObject *inOwner)
    {
        m_Owner = inOwner;
    }

    void Component::MarkForNeededEndOfFrameUpdate()
    {
        RuntimeGlobalContext::Get().m_SceneManager->MarkComponentForNeededEndOfFrameUpdate(this);
    }
}
