#include "GameObject.h"

namespace minEngine
{
    GameObject::GameObject(uint64_t id, std::string name)
        : m_ID(id)
    {
        if (name.empty())
        {
            m_Name = "GameObject_" + std::to_string(m_ID);
        }
        else
        {
            m_Name = std::move(name);
        }
    }

    void GameObject::Tick(float deltaTime)
    {
        for (auto& component : m_Components)
        {
            component->Tick(deltaTime);
        }
    }
}