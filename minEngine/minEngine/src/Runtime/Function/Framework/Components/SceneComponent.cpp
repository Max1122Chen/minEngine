#include "SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    SceneComponent::SceneComponent(std::shared_ptr<GameObject> owner)
        : Component(owner)
    {
        if(owner->GetRootComponent().get() != nullptr)
        {
            m_Transform = owner->GetTransform();
        }
    }

}

