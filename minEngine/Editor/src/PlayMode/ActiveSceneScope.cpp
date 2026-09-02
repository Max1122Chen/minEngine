#include "ActiveSceneScope.h"

#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

namespace minEngine
{
    ActiveSceneScope::ActiveSceneScope(Scene* pieScene)
    {
        if (!SceneManager::HasInstance())
        {
            return;
        }

        m_PreviousScene = SceneManager::Get().GetActiveSceneOverride();
        SceneManager::Get().SetActiveSceneOverride(pieScene);
    }

    ActiveSceneScope::~ActiveSceneScope()
    {
        if (!SceneManager::HasInstance())
        {
            return;
        }

        SceneManager::Get().SetActiveSceneOverride(m_PreviousScene);
    }
}
