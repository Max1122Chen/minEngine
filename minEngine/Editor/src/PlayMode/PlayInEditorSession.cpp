#include "PlayInEditorSession.h"

#include "ActiveSceneScope.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Audio/AudioSystem.h"
#include "Runtime/Function/Framework/Scene/SceneDuplicator.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "Shell/EditorContextHelpers.h"

namespace minEngine
{
    namespace
    {
        // MVP: single PIE instance; must match SceneManager::GetTickTargetScene PIE lookup.
        constexpr int32_t kPIEInstanceId = 0;
    }

    Scene* PlayInEditorSession::GetEditorScene() const
    {
        if (m_EditorContext.Scene)
        {
            return m_EditorContext.Scene.get();
        }

        if (SceneManager::HasInstance())
        {
            return SceneManager::Get().GetEditorScene();
        }

        return nullptr;
    }

    Scene* PlayInEditorSession::GetPIEScene() const
    {
        if (!m_PIEContexts.empty() && m_PIEContexts[0].Scene)
        {
            return m_PIEContexts[0].Scene.get();
        }

        return nullptr;
    }

    const SceneContext* PlayInEditorSession::GetEditorContext() const
    {
        return m_EditorContext.Scene ? &m_EditorContext : nullptr;
    }

    const SceneContext* PlayInEditorSession::GetPIEContext(int32_t instanceId) const
    {
        for (const SceneContext& context : m_PIEContexts)
        {
            if (context.PIEInstanceId == instanceId)
            {
                return &context;
            }
        }

        return nullptr;
    }

    Scene* PlayInEditorSession::GetTickTargetScene() const
    {
        if (m_State == PlayState::Playing)
        {
            return GetPIEScene();
        }

        return GetEditorScene();
    }

    bool PlayInEditorSession::EnterPlay()
    {
        if (m_State == PlayState::Playing || m_State == PlayState::Stopping)
        {
            return false;
        }

        if (!SceneManager::HasInstance())
        {
            return false;
        }

        SceneManager& sceneManager = SceneManager::Get();
        std::shared_ptr<Scene> editorScene = sceneManager.GetCurrentActiveScene();
        if (!editorScene)
        {
            ME_CORE_WARN("PlayInEditorSession::EnterPlay: no active editor scene.");
            return false;
        }

        m_EditorContext.Type = ESceneType::Editor;
        m_EditorContext.TickPolicy = ESceneTickPolicy::None;
        m_EditorContext.PIEInstanceId = -1;
        m_EditorContext.Scene = editorScene;
        m_EditorContext.ContextHandle = "Editor";
        editorScene->SetSceneType(ESceneType::Editor);
        editorScene->SetTickPolicy(ESceneTickPolicy::None);

        m_CloneContext = {};
        m_CloneContext.PIEInstanceId = kPIEInstanceId;
        m_CloneContext.TargetType = ESceneType::PIE;

        std::shared_ptr<Scene> pieScene = SceneDuplicator::DuplicateForPIE(*editorScene, m_CloneContext);
        if (!pieScene)
        {
            editorScene->SetTickPolicy(ESceneTickPolicy::Gameplay);
            m_EditorContext.TickPolicy = ESceneTickPolicy::Gameplay;
            return false;
        }

        pieScene->SetPIEInstanceId(kPIEInstanceId);

        SceneContext pieContext;
        pieContext.Type = ESceneType::PIE;
        pieContext.TickPolicy = ESceneTickPolicy::Gameplay;
        pieContext.PIEInstanceId = kPIEInstanceId;
        pieContext.Scene = pieScene;
        pieContext.ContextHandle = "PIE_0";
        m_PIEContexts.clear();
        m_PIEContexts.push_back(std::move(pieContext));

        m_ObjectMapping.Build(m_CloneContext);

        sceneManager.SetEditorSceneContext(m_EditorContext);
        sceneManager.RegisterPIEScene(pieScene, kPIEInstanceId);
        sceneManager.SetPIEPlayActive(true);

        if (RenderScene* editorRenderScene = editorScene->GetRenderScene())
        {
            editorRenderScene->CollectOrphanedSceneProxies();
        }

        if (AudioSystem::HasInstance())
        {
            AudioSystem::Get().OnBeginPIE(pieScene.get());
        }

        if (PhysicsSystem::HasInstance())
        {
            PhysicsSystem::Get().OnBeginPIE(pieScene.get());
        }

        m_State = PlayState::Playing;
        ApplyInspectingSceneForPlayState();
        return true;
    }

    void PlayInEditorSession::Stop()
    {
        if (m_State != PlayState::Playing)
        {
            return;
        }

        m_State = PlayState::Stopping;

        Scene* pieScene = GetPIEScene();

        if (SceneManager::HasInstance())
        {
            SceneManager& sceneManager = SceneManager::Get();
            sceneManager.SetPIEPlayActive(false);

            if (AudioSystem::HasInstance() && pieScene != nullptr)
            {
                AudioSystem::Get().OnEndPIE(pieScene);
            }

            if (PhysicsSystem::HasInstance() && pieScene != nullptr)
            {
                PhysicsSystem::Get().OnEndPIE(pieScene);
            }

            for (const SceneContext& pieContext : m_PIEContexts)
            {
                sceneManager.UnregisterPIEScene(pieContext.PIEInstanceId);
            }
        }

        m_PIEContexts.clear();
        m_ObjectMapping.Clear();
        m_CloneContext = {};

        if (m_EditorContext.Scene)
        {
            m_EditorContext.TickPolicy = ESceneTickPolicy::Gameplay;
            m_EditorContext.Scene->SetTickPolicy(ESceneTickPolicy::Gameplay);

            if (RenderScene* editorRenderScene = m_EditorContext.Scene->GetRenderScene())
            {
                editorRenderScene->CollectOrphanedSceneProxies();
            }

            if (SceneManager::HasInstance())
            {
                SceneManager::Get().SetEditorSceneContext(m_EditorContext);
            }

            if (ObjectManager::HasInstance())
            {
                ObjectManager::Get().CollectGarbage();
            }
        }

        m_State = PlayState::Editing;
        ApplyInspectingSceneForPlayState();
    }

    void PlayInEditorSession::TickPIE(float deltaTime)
    {
        (void)deltaTime;
        if (m_State != PlayState::Playing)
        {
            return;
        }

        ActiveSceneScope activeScope(GetPIEScene());
    }

    void PlayInEditorSession::SyncSceneContextsToManager()
    {
        if (!SceneManager::HasInstance())
        {
            return;
        }

        SceneManager::Get().SetEditorSceneContext(m_EditorContext);
    }

    void PlayInEditorSession::ApplyInspectingSceneForPlayState()
    {
        if (m_HostContext == nullptr)
        {
            return;
        }

        if (m_State == PlayState::Playing)
        {
            m_HostContext->SetInspectingScene(GetPIEScene());
        }
        else
        {
            Scene* editorScene = GetEditorScene();
            if (editorScene == nullptr && SceneManager::HasInstance())
            {
                editorScene = SceneManager::Get().GetEditorScene();
            }
            m_HostContext->SetInspectingScene(editorScene);
        }

        if (SceneEditor* sceneEditor = GetSceneEditor(m_HostContext))
        {
            sceneEditor->ClearSelectedGameObject();
        }
    }
}
