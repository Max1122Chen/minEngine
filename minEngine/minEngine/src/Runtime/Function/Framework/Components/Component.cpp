#include "Component.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Core/Object/ObjectCast.h"

namespace minEngine
{
    Component::Component() = default;

    Component::~Component()
    {
        if (m_bFixedTickEnabled)
        {
            SetFixedTickEnabled(false);
        }
        if (SceneManager::HasInstance())
        {
            SceneManager::Get().UnmarkComponentForNeededEndOfFrameUpdate(this);
        }
        Deactivate();
    }

    void Component::SetFixedTickEnabled(bool enabled)
    {
        if (m_bFixedTickEnabled == enabled)
        {
            return;
        }

        m_bFixedTickEnabled = enabled;
        if (m_Owner != nullptr)
        {
            m_Owner->NotifyFixedTickComponentChanged(enabled);
        }
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

        return Cast<Scene>(const_cast<MEObject*>(m_Owner->GetOuter()));
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
        // Intent: the dedupe compares the author's flag, never the hierarchy result.
        if (IsActiveSelf() == active)
        {
            return;
        }

        m_bActive = active;
        m_EffectiveActiveCache = kEffectiveStateUnknown;
        SyncActivationWithActiveFlag();
    }

    void Component::SyncActivationWithActiveFlag()
    {
        // Permission to register follows the effective state; intent follows the authored flag.
        const bool want = IsActive();
        if (want == m_bAppliedEffective)
        {
            if (!want)
            {
                // Off means deliberately off, not "waiting for a Scene", so pending stays clear.
                m_bPendingActivation = false;
            }
            return;
        }

        m_bAppliedEffective = want;
        if (want)
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
        // Intent: "does the author still want it on". A parent that is off must not cancel the
        // intent, otherwise re-enabling the parent loses the author's choice.
        if (!IsActiveSelf() || !m_bPendingActivation)
        {
            return;
        }

        TryActivate();
    }

    void Component::TryActivate()
    {
        // Permission: an ancestor that is off must never reach ApplyActivationToSystems.
        if (!IsActive())
        {
            return;
        }

        if (!CanApplyActivation())
        {
            m_bPendingActivation = true;
            m_bAppliedEffective = false;
            return;
        }

        if (m_bActivationApplied)
        {
            m_bPendingActivation = false;
            return;
        }

        ApplyActivationToSystems();
        ++m_ActivationApplyCount;
        OnActivate();
        m_bActivationApplied = true;
        m_bAppliedEffective = true;
        m_bPendingActivation = false;
        TryBeginPlayIfSimulating();
    }

    void Component::Deactivate()
    {
        if (m_bActivationApplied)
        {
            OnDeactivate();
            RemoveActivationFromSystems();
            ++m_ActivationRemoveCount;
            m_bActivationApplied = false;
        }

        m_bPendingActivation = false;
        m_bAppliedEffective = false;
    }

    void Component::BeginPlay()
    {
        if (m_bHasBegunPlay || !m_bActivationApplied)
        {
            return;
        }

        OnBeginPlay();
        m_bHasBegunPlay = true;
    }

    void Component::EndPlay(EEndPlayReason reason)
    {
        if (!m_bHasBegunPlay)
        {
            return;
        }

        OnEndPlay(reason);
        m_bHasBegunPlay = false;
    }

    void Component::TryBeginPlayIfSimulating()
    {
        if (m_bHasBegunPlay || !m_bActivationApplied)
        {
            return;
        }

        Scene* scene = GetOwningScene();
        if (scene == nullptr || !scene->HasBegunPlay())
        {
            return;
        }

        if (m_Owner == nullptr)
        {
            return;
        }

        if (!m_Owner->HasBegunPlay())
        {
            m_Owner->BeginPlay();
            return;
        }

        BeginPlay();
    }

    void Component::SetOwner(GameObject* inOwner)
    {
        if (m_Owner == inOwner)
        {
            return;
        }

        Deactivate();
        m_Owner = inOwner;

        // A new owner means a new ancestor chain, so republish it and reconcile.
        m_bOwnerEffectiveActive = (inOwner != nullptr) ? inOwner->IsActive() : true;
        m_EffectiveActiveCache = kEffectiveStateUnknown;
        SyncActivationWithActiveFlag();
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
        if (SceneManager::HasInstance())
        {
            SceneManager::Get().MarkComponentForNeededEndOfFrameUpdate(this);
        }
    }
}