#include "GameObject.h"

namespace minEngine
{
    GameObject::GameObject()
    {}

    Transform GameObject::GetTransform()
    {
        return m_RootComponent ? m_RootComponent->GetTransform() : Transform();
    }

    void GameObject::SetTransform(const Transform &inTransform)
    {
        if (m_RootComponent)
        {
            m_RootComponent->SetTransform(inTransform);
        }
    }

    Vector3 GameObject::GetPosition()
    {
        return m_RootComponent ? m_RootComponent->GetPosition() : Vector3();
    }

    void GameObject::SetPosition(const Vector3 &position)
    {
        if (m_RootComponent)
        {
            m_RootComponent->SetPosition(position);
        }
    }

    Vector3 GameObject::GetRotation()
    {
        return m_RootComponent ? m_RootComponent->GetRotation() : Vector3();
    }

    void GameObject::SetRotation(const Vector3 &rotation)
    {
        if (m_RootComponent)
        {
            m_RootComponent->SetRotation(rotation);
        }
    }

    Vector3 GameObject::GetScale()
    {
        return m_RootComponent ? m_RootComponent->GetScale() : Vector3();
    }

    void GameObject::SetScale(const Vector3 &scale)
    {
        if (m_RootComponent)
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