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

    Quaternion GameObject::GetRotation()
    {
        return m_RootComponent ? m_RootComponent->GetRotation() : Quaternion::Identity();
    }

    void GameObject::SetRotation(const Quaternion& rotation)
    {
        if (m_RootComponent)
        {
            m_RootComponent->SetRotation(rotation);
        }
    }

    Vector3 GameObject::GetRotationEulerDegrees()
    {
        return m_RootComponent ? m_RootComponent->GetRotationEulerDegrees() : Vector3();
    }

    void GameObject::SetRotationEulerDegrees(const Vector3& rotationEulerDegrees)
    {
        if (m_RootComponent)
        {
            m_RootComponent->SetRotationEulerDegrees(rotationEulerDegrees);
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
        AddComponent_Internal(newComponent);
        return newComponent;
    }

    bool GameObject::RemoveComponent(Component& target)
    {
        auto it = std::find_if(m_Components.begin(),m_Components.end(),[&target](const std::shared_ptr<Component>& componentPtr)
        {
            return componentPtr.get() == &target;
        });
        if ( it != m_Components.end())
        {
            // Handle the case if the component to remove is a SceneComponent
            if(target.GetClass() && target.IsA(SceneComponent::StaticClass()))
            {
                SceneComponent* sceneComponent = static_cast<SceneComponent*>(it->get());
                if (sceneComponent == m_RootComponent)
                {
                    m_RootComponent = nullptr;
                    // Here we try to find another SceneComponent to be the new RootComponent, and reattach other SceneComponents to it
                    // First we try to find the first child SceneComponent of the removed RootComponent to be the new RootComponent, and reattach other child SceneComponents to it
                    SceneComponent* removedRoot = sceneComponent;
                    SceneComponent* newRootCandidate = nullptr;
                    for (SceneComponent* child : removedRoot->GetAttachChildren())
                    {
                        if (child != removedRoot)
                        {
                            newRootCandidate = child;
                            break;
                        }
                    }
                    if(newRootCandidate)
                    {
                        SetRootComponent(newRootCandidate);
                    }
                    // If there is no child SceneComponent, we find the first SceneComponent in the components list to be the new RootComponent
                    else
                    {
                        for (auto& component : m_Components)
                        {
                            if (component->GetClass() && component->IsA(SceneComponent::StaticClass()))
                            {
                                newRootCandidate = std::static_pointer_cast<SceneComponent>(component).get();
                                break;
                            }
                        }
                        if(newRootCandidate)
                        {
                            SetRootComponent(newRootCandidate);
                        }
                    }
                    // Finally we reattach all other SceneComponents to the new RootComponent
                    if(newRootCandidate)
                    {
                        for (SceneComponent* child : removedRoot->GetAttachChildren())
                        {
                            if (child != removedRoot && child != newRootCandidate)
                            {
                                child->AttachToComponent(newRootCandidate, AttachmentTransformRules::KeepRelativeTransform);
                            }
                        }
                    }
                }
            }
            m_Components.erase(it);
            return true;
        }
        return false;
    }

    void GameObject::InsertRestoredComponent(std::shared_ptr<Component> component, size_t index)
    {
        if (!component)
        {
            return;
        }

        component->SetOwner(this);
        if (index >= m_Components.size())
        {
            m_Components.push_back(component);
        }
        else
        {
            m_Components.insert(m_Components.begin() + static_cast<std::ptrdiff_t>(index), component);
        }

        if (!component->GetClass() || !component->IsA(SceneComponent::StaticClass()))
        {
            return;
        }

        SceneComponent* sceneComponent = static_cast<SceneComponent*>(component.get());
        if (m_RootComponent == nullptr)
        {
            m_RootComponent = sceneComponent;
        }
    }

    void GameObject::AddComponent_Internal(std::shared_ptr<Component> newComponent)
    {
        if(!newComponent)
        {
            return;
        }
        newComponent->SetOwner(this);
        m_Components.push_back(newComponent);
        if (newComponent->GetClass() && newComponent->IsA(SceneComponent::StaticClass()))
        {
            SceneComponent* sceneComponent = static_cast<SceneComponent*>(newComponent.get());
            if (!m_RootComponent)
            {
                m_RootComponent = sceneComponent;
            }
            else
            {
                sceneComponent->AttachToComponent(m_RootComponent, AttachmentTransformRules::KeepRelativeTransform);
            }
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