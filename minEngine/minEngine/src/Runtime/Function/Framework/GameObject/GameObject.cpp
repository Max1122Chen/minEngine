#include "GameObject.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include <algorithm>

namespace minEngine
{
    GameObject::GameObject()
    {}

    GameObject::~GameObject()
    {
        UnlinkFromCurrentParent();

        for (GameObject* child : m_Children)
        {
            if (child == nullptr)
            {
                continue;
            }
            child->m_Parent = nullptr;
            if (SceneComponent* childRoot = child->GetRootComponent())
            {
                if (childRoot->GetAttachParent() != nullptr)
                {
                    childRoot->DetachFromParent(AttachmentTransformRules::KeepWorldTransform);
                }
            }
        }
        m_Children.clear();

        for (auto& component : m_Components)
        {
            if (component)
            {
                component->SetOwner(nullptr);
                component.reset();
            }
        }
        m_Components.clear();
        ME_CORE_INFO("GameObject with ID {} and name '{}' is being destroyed.", m_ID, GetName());
    }

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

    Transform GameObject::GetWorldTransform() const
    {
        return m_RootComponent ? m_RootComponent->GetWorldTransform() : Transform();
    }

    void GameObject::SetWorldTransform(const Transform& worldTransform)
    {
        if (m_RootComponent)
        {
            m_RootComponent->SetWorldTransform(worldTransform);
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
                    // Prefer a former attach-child of the removed Root as the new Root.
                    SceneComponent* removedRoot = sceneComponent;
                    SceneComponent* newRootCandidate = nullptr;
                    for (SceneComponent* child : removedRoot->GetAttachChildren())
                    {
                        if (child != nullptr && child != removedRoot)
                        {
                            newRootCandidate = child;
                            break;
                        }
                    }
                    if (newRootCandidate == nullptr)
                    {
                        for (auto& component : m_Components)
                        {
                            if (component.get() == &target)
                            {
                                continue;
                            }
                            if (component->GetClass() && component->IsA(SceneComponent::StaticClass()))
                            {
                                newRootCandidate = std::static_pointer_cast<SceneComponent>(component).get();
                                break;
                            }
                        }
                    }

                    // Promotes Root and rebinds GO-parent / child-GO Roots (CORE-F09).
                    SetRootComponent(newRootCandidate);

                    if (newRootCandidate != nullptr)
                    {
                        const std::vector<SceneComponent*> formerChildren = removedRoot->GetAttachChildren();
                        for (SceneComponent* child : formerChildren)
                        {
                            if (child != nullptr && child != removedRoot && child != newRootCandidate)
                            {
                                child->AttachToComponent(
                                    newRootCandidate, AttachmentTransformRules::KeepRelativeTransform);
                            }
                        }
                    }
                }
                else if (sceneComponent->GetAttachParent() != nullptr)
                {
                    sceneComponent->DetachFromParent(AttachmentTransformRules::KeepWorldTransform);
                }
            }

            if (SceneManager::HasInstance())
            {
                SceneManager::Get().UnmarkComponentForNeededEndOfFrameUpdate(&target);
            }

            // Deactivate while still owned; destructor will also unlink attach safely.
            target.SetOwner(nullptr);
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
            if (component && component->IsActive())
            {
                component->Tick(deltaTime);
            }
        }
    }

    void GameObject::UnlinkFromCurrentParent()
    {
        if (m_Parent == nullptr)
        {
            return;
        }

        auto& siblings = m_Parent->m_Children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        m_Parent = nullptr;
    }

    void GameObject::ClearChildrenLinks()
    {
        m_Children.clear();
    }

    bool GameObject::WouldCreateHierarchyCycle(const GameObject* candidateParent) const
    {
        const GameObject* cursor = candidateParent;
        while (cursor != nullptr)
        {
            if (cursor == this)
            {
                return true;
            }
            cursor = cursor->m_Parent;
        }
        return false;
    }

    void GameObject::SetRootComponent(SceneComponent* newRoot)
    {
        if (m_RootComponent == newRoot)
        {
            return;
        }

        SceneComponent* oldRoot = m_RootComponent;

        // Promoting an attach-child of the old Root: detach KeepWorld first.
        if (newRoot != nullptr && oldRoot != nullptr && newRoot->GetAttachParent() == oldRoot)
        {
            newRoot->DetachFromParent(AttachmentTransformRules::KeepWorldTransform);
        }

        // Break old Root's GO-hierarchy attach under the parent Root before swap.
        if (oldRoot != nullptr && m_Parent != nullptr)
        {
            SceneComponent* parentRoot = m_Parent->GetRootComponent();
            if (parentRoot != nullptr && oldRoot->GetAttachParent() == parentRoot)
            {
                oldRoot->DetachFromParent(AttachmentTransformRules::KeepWorldTransform);
            }
        }

        m_RootComponent = newRoot;

        RebindRootToGameObjectParent(AttachmentTransformRules::KeepWorldTransform);
        RebindChildGameObjectRoots(AttachmentTransformRules::KeepWorldTransform);
    }

    void GameObject::RebindRootToGameObjectParent(AttachmentTransformRules rules)
    {
        if (m_Parent == nullptr)
        {
            return;
        }

        SceneComponent* childRoot = GetRootComponent();
        SceneComponent* parentRoot = m_Parent->GetRootComponent();
        if (childRoot == nullptr || parentRoot == nullptr)
        {
            ME_CORE_WARN(
                "GameObject::RebindRootToGameObjectParent: missing Root on '{}' or parent '{}'; dissolving GO edge.",
                GetName(),
                m_Parent->GetName());
            UnlinkFromCurrentParent();
            return;
        }

        if (childRoot->GetAttachParent() != parentRoot)
        {
            childRoot->AttachToComponent(parentRoot, rules);
        }
    }

    void GameObject::RebindChildGameObjectRoots(AttachmentTransformRules rules)
    {
        SceneComponent* selfRoot = GetRootComponent();
        const std::vector<GameObject*> childrenCopy = m_Children;
        for (GameObject* child : childrenCopy)
        {
            if (child == nullptr)
            {
                continue;
            }

            SceneComponent* childRoot = child->GetRootComponent();
            if (selfRoot == nullptr || childRoot == nullptr)
            {
                child->DetachFromParent(rules);
                continue;
            }

            if (childRoot->GetAttachParent() != selfRoot)
            {
                childRoot->AttachToComponent(selfRoot, rules);
            }
        }
    }

    bool GameObject::AttachToParent(GameObject* parent, AttachmentTransformRules rules)
    {
        if (parent == nullptr || parent == this)
        {
            ME_CORE_ERROR("GameObject::AttachToParent: invalid parent for GO '{}'.", GetName());
            return false;
        }

        if (WouldCreateHierarchyCycle(parent))
        {
            ME_CORE_ERROR(
                "GameObject::AttachToParent: cycle detected attaching '{}' under '{}'.",
                GetName(),
                parent->GetName());
            return false;
        }

        SceneComponent* childRoot = GetRootComponent();
        SceneComponent* parentRoot = parent->GetRootComponent();
        if (childRoot == nullptr || parentRoot == nullptr)
        {
            ME_CORE_ERROR(
                "GameObject::AttachToParent: both GOs need a Root SceneComponent ('{}' under '{}').",
                GetName(),
                parent->GetName());
            return false;
        }

        if (m_Parent == parent)
        {
            auto& siblings = parent->m_Children;
            if (std::find(siblings.begin(), siblings.end(), this) == siblings.end())
            {
                siblings.push_back(this);
            }
            if (childRoot->GetAttachParent() != parentRoot)
            {
                childRoot->AttachToComponent(parentRoot, rules);
            }
            return true;
        }

        DetachFromParent(AttachmentTransformRules::KeepWorldTransform);

        m_Parent = parent;
        parent->m_Children.push_back(this);
        childRoot->AttachToComponent(parentRoot, rules);
        return true;
    }

    void GameObject::DetachFromParent(AttachmentTransformRules rules)
    {
        if (m_Parent != nullptr)
        {
            UnlinkFromCurrentParent();
        }

        if (SceneComponent* childRoot = GetRootComponent())
        {
            if (childRoot->GetAttachParent() != nullptr)
            {
                childRoot->DetachFromParent(rules);
            }
        }
    }

}
