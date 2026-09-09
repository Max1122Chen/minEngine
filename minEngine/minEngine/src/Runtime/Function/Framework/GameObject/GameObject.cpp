#include "GameObject.h"

#include "Runtime/Core/Reflection/Reflection.h"

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

    Component* GameObject::FindComponentByClassName(const std::string& componentClassName) const
    {
        if (componentClassName.empty())
        {
            return nullptr;
        }

        const Reflection::ReflectionSystem& reflection = Reflection::ReflectionSystem::Get();
        const Reflection::MEClass* targetClass = reflection.FindClass(componentClassName);
        if (targetClass == nullptr)
        {
            for (const std::shared_ptr<Component>& component : m_Components)
            {
                if (component == nullptr || component->GetClass() == nullptr)
                {
                    continue;
                }

                const std::string& className = component->GetClass()->GetName();
                const size_t scopePos = className.rfind("::");
                const std::string shortName =
                    scopePos != std::string::npos ? className.substr(scopePos + 2) : className;
                if (shortName == componentClassName)
                {
                    targetClass = component->GetClass();
                    break;
                }
            }
        }

        if (targetClass == nullptr)
        {
            return nullptr;
        }

        for (const std::shared_ptr<Component>& component : m_Components)
        {
            if (component != nullptr && component->IsA(targetClass))
            {
                return component.get();
            }
        }

        return nullptr;
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

    size_t GameObject::FindComponentIndex(const Component& target) const
    {
        for (size_t index = 0; index < m_Components.size(); ++index)
        {
            if (m_Components[index] && m_Components[index].get() == &target)
            {
                return index;
            }
        }
        return m_Components.size();
    }

    bool GameObject::MoveComponent(Component& target, size_t newIndex)
    {
        if (m_RootComponent != nullptr && &target == m_RootComponent)
        {
            return false;
        }

        const size_t count = m_Components.size();
        const size_t oldIndex = FindComponentIndex(target);
        if (oldIndex >= count)
        {
            return false;
        }

        if (newIndex >= count)
        {
            newIndex = count - 1;
        }

        // Keep Root pinned (Design ED-F05 §10.4): non-root may not land at/before Root.
        if (m_RootComponent != nullptr)
        {
            const size_t rootIndex = FindComponentIndex(*m_RootComponent);
            if (rootIndex < count)
            {
                const size_t minIndex = rootIndex + 1;
                if (minIndex >= count)
                {
                    return false;
                }
                if (newIndex < minIndex)
                {
                    newIndex = minIndex;
                }
            }
        }

        if (oldIndex == newIndex)
        {
            return true;
        }

        std::shared_ptr<Component> held = m_Components[oldIndex];
        m_Components.erase(m_Components.begin() + static_cast<std::ptrdiff_t>(oldIndex));
        if (newIndex > m_Components.size())
        {
            newIndex = m_Components.size();
        }
        m_Components.insert(m_Components.begin() + static_cast<std::ptrdiff_t>(newIndex), held);
        return true;
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
            if (component && component->IsActive())
            {
                component->Tick(deltaTime);
            }
        }
    }

}