#include "GameObject.h"

namespace minEngine
{
    GameObject::GameObject(uint64_t id)
        : m_ID(id)
    {}

    void GameObject::Tick(float deltaTime)
    {
        for (auto& component : m_Components)
        {
            component->Tick(deltaTime);
        }
    }
}