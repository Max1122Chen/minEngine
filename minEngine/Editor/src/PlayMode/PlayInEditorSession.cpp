#include "PlayInEditorSession.h"

#include "ActiveSceneScope.h"

#include "Runtime/Function/Framework/Scene/SceneDuplicator.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Physics/PhysicsSystem.h"

namespace minEngine
{
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
        m_CloneContext.PIEInstanceId = m_NextPIEInstanceId;
        m_CloneContext.TargetType = ESceneType::PIE;

        std::shared_ptr<Scene> pieScene = SceneDuplicator::DuplicateForPIE(*editorScene, m_CloneContext);
        if (!pieScene)
        {
            editorScene->SetTickPolicy(ESceneTickPolicy::Gameplay);
            m_EditorContext.TickPolicy = ESceneTickPolicy::Gameplay;
            return false;
        }

        pieScene->SetPIEInstanceId(m_NextPIEInstanceId);

        SceneContext pieContext;
        pieContext.Type = ESceneType::PIE;
        pieContext.TickPolicy = ESceneTickPolicy::Gameplay;
        pieContext.PIEInstanceId = m_NextPIEInstanceId;
        pieContext.Scene = pieScene;
        pieContext.ContextHandle = "PIE_0";
        m_PIEContexts.clear();
        m_PIEContexts.push_back(std::move(pieContext));

        m_ObjectMapping.Build(m_CloneContext);

        sceneManager.SetEditorSceneContext(m_EditorContext);
        sceneManager.RegisterPIEScene(pieScene, m_NextPIEInstanceId);
        sceneManager.SetPIEPlayActive(true);

        if (PhysicsSystem::HasInstance())
        {
            PhysicsSystem::Get().GetOrCreateWorld(pieScene.get());
            PhysicsSystem::Get().RebuildWorldBodies(pieScene.get());
        }

        m_State = PlayState::Playing;
        ++m_NextPIEInstanceId;
        return true;
    }

    void PlayInEditorSession::Stop()
    {
        if (m_State != PlayState::Playing)
        {
            return;
        }

        m_State = PlayState::Stopping;

        if (SceneManager::HasInstance())
        {
            SceneManager& sceneManager = SceneManager::Get();
            sceneManager.SetPIEPlayActive(false);
            sceneManager.UnregisterPIEScene(0);
        }

        m_PIEContexts.clear();
        m_ObjectMapping.Clear();
        m_CloneContext = {};

        if (m_EditorContext.Scene)
        {
            m_EditorContext.TickPolicy = ESceneTickPolicy::Gameplay;
            m_EditorContext.Scene->SetTickPolicy(ESceneTickPolicy::Gameplay);

            if (SceneManager::HasInstance())
            {
                SceneManager::Get().SetEditorSceneContext(m_EditorContext);
            }
        }

        m_State = PlayState::Editing;
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
}
