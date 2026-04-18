#include "GameObject.h"

namespace minEngine
{
    GameObject::GameObject()
    {}

    const Transform &GameObject::GetTransform()
    {
        return m_RootComponent->GetTransform();
    }

    void GameObject::SetTransform(const Transform &inTransform)
    {
        m_RootComponent->SetTransform(inTransform);
    }

    const Vector3 &GameObject::GetPosition()
    {
        return m_RootComponent->GetPosition();
    }

    void GameObject::SetPosition(const Vector3 &position)
    {
        m_RootComponent->SetPosition(position);
    }

    const Vector3 &GameObject::GetRotation()
    {
        return m_RootComponent->GetRotation();
    }

    void GameObject::SetRotation(const Vector3 &rotation)
    {
        m_RootComponent->SetRotation(rotation);
    }

    const Vector3 &GameObject::GetScale()
    {
        return m_RootComponent->GetScale();
    }

    void GameObject::SetScale(const Vector3 &scale)
    {
        m_RootComponent->SetScale(scale);
    }

    void GameObject::Tick(float deltaTime)
    {
        for (auto& component : m_Components)
        {
            component->Tick(deltaTime);
        }
    }

}