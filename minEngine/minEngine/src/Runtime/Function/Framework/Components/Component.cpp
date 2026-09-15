#include "Component.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/RenderScene.h"

namespace minEngine
{
    Component::Component() = default;

    Component::~Component()
    {
        if (SceneManager::HasInstance())
        {
            SceneManager::Get().UnmarkComponentForNeededEndOfFrameUpdate(this);
        }
        Deactivate();
    }

    bool Component::CanApplyActivation() const
    {
        return GetOwningScene() != nullptr;
    }

    Scene* Component::GetOwningScene() const
    {
        if (m_Owner == nullptr)
        {
            return nullptr;
        }

        return dynamic_cast<Scene*>(const_cast<MEObject*>(m_Owner->GetOuter()));
    }

    RenderScene* Component::GetOwningRenderScene() const
    {
        Scene* scene = GetOwningScene();
        if (scene == nullptr)
        {
            return nullptr;
        }

        return scene->GetRenderScene();
    }

    RenderScene* Component::GetOwningRenderSceneIfPresent() const
    {
        Scene* scene = GetOwningScene();
        if (scene == nullptr)
        {
            return nullptr;
        }

        return scene->GetRenderSceneShared().get();
    }

    void Component::SetActive(bool active)
    {
        if (m_bActive == active)
        {
            return;
        }

        m_bActive = active;
        SyncActivationWithActiveFlag();
    }

    void Component::SyncActivationWithActiveFlag()
    {
        if (m_bActive)
        {
            TryActivate();
        }
        else
        {
            Deactivate();
        }
    }

    void Component::ResolvePendingActivation()
    {
        if (!m_bActive || !m_bPendingActivation)
        {
            return;
        }

        TryActivate();
    }

    void Component::TryActivate()
    {
        if (!m_bActive)
        {
            return;
        }

        if (!CanApplyActivation())
        {
            m_bPendingActivation = true;
            return;
        }

        if (m_bActivationApplied)
        {
            m_bPendingActivation = false;
            return;
        }

        ApplyActivationToSystems();
        OnActivate();
        m_bActivationApplied = true;
        m_bPendingActivation = false;
    }

    void Component::Deactivate()
    {
        if (m_bActivationApplied)
        {
            OnDeactivate();
            RemoveActivationFromSystems();
            m_bActivationApplied = false;
        }

        m_bPendingActivation = false;
    }

    void Component::SetOwner(GameObject* inOwner)
    {
        if (m_Owner == inOwner)
        {
            return;
        }

        Deactivate();
        m_Owner = inOwner;

        if (m_bActive)
        {
            TryActivate();
        }
    }

    void Component::OnActivate()
    {
    }

    void Component::OnDeactivate()
    {
    }

    void Component::ApplyActivationToSystems()
    {
    }

    void Component::RemoveActivationFromSystems()
    {
    }

    void Component::MarkForNeededEndOfFrameUpdate()
    {
        SceneManager::Get().MarkComponentForNeededEndOfFrameUpdate(this);
    }
}
