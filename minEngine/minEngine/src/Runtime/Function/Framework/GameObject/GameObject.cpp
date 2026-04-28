#include "GameObject.h"
#include "Core/Reflection/Reflection.h"
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

    void GameObject::Translate(const Vector3 &delta)
    {
        if (m_RootComponent)
        {
            m_RootComponent->Translate(delta);
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

    void GameObject::Rotate(const glm::quat &delta, Space relativeTo)
    {
        if (m_RootComponent)
        {
            m_RootComponent->Rotate(delta, relativeTo);
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

    void GameObject::ScaleBy(const Vector3 &scaleFactor)
    {
        if (m_RootComponent)
        {
            m_RootComponent->ScaleBy(scaleFactor);
        }
    }

    std::shared_ptr<Component> GameObject::AddComponent(const std::string &componentTypeName)
    {
        ObjectManager& objectManager = ObjectManager::Get();
        std::shared_ptr<MEObject> newComponentBase = objectManager.NewObject(componentTypeName, "", this);
        if (!newComponentBase)        
        {
            return nullptr;
        }
        std::shared_ptr<Component> newComponent = std::static_pointer_cast<Component>(newComponentBase);
        newComponent->SetOwner(this);
        m_Components.push_back(newComponent);
        Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();
        if (newComponent->GetClass() && reflectionSystem.IsClassSameOrDerived(newComponent->GetClass(), reflectionSystem.FindClass<SceneComponent>()))
        {
            // Set the first added SceneComponent as the RootComponent by default
            if (!m_RootComponent)
            {
                m_RootComponent = std::static_pointer_cast<SceneComponent>(newComponent);
            }
            else
            {
                SceneComponent* sceneComponent = static_cast<SceneComponent*>(newComponent.get());
                sceneComponent->AttachToComponent(m_RootComponent.get(), AttachmentTransformRules::KeepRelativeTransform);
            }
        }
        return newComponent;
    }

    void GameObject::Tick(float deltaTime)
    {
        for (auto& component : m_Components)
        {
            component->Tick(deltaTime);
        }
    }

}