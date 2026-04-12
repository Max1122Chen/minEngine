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

    const Transform &GameObject::GetTransform()
    {
        if(m_RootComponent)
        {
            return m_RootComponent->GetTransform();
        }
        else
        {
            return Transform();
        }
    }

    void GameObject::SetTransform(const Transform &inTransform)
    {
        if(m_RootComponent)
        {
            m_RootComponent->SetTransform(inTransform);
        }
    }

    const Vector3 &GameObject::GetPosition()
    {
        if(m_RootComponent)
        {
            return m_RootComponent->GetPosition();
        }
        else
        {
            return Vector3();
        }
    }

    void GameObject::SetPosition(const Vector3 &position)
    {
        if(m_RootComponent)
        {
            m_RootComponent->SetPosition(position);
        }
    }

    const Vector3 &GameObject::GetRotation()
    {
        if(m_RootComponent)
        {
            return m_RootComponent->GetRotation();
        }
        else
        {
            return Vector3();
        }
    }

    void GameObject::SetRotation(const Vector3 &rotation)
    {
        if(m_RootComponent)
        {
            m_RootComponent->SetRotation(rotation);
        }
    }

    const Vector3 &GameObject::GetScale()
    {
        if(m_RootComponent)
        {
            return m_RootComponent->GetScale();
        }
        else
        {
            return Vector3();
        }
    }

    void GameObject::SetScale(const Vector3 &scale)
    {
        if(m_RootComponent)
        {
            m_RootComponent->SetScale(scale);
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