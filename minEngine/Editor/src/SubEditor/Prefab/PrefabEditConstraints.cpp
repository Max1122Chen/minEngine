#include "SubEditor/Prefab/PrefabEditConstraints.h"

#include "SubEditor/Prefab/PrefabStageController.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

namespace minEngine
{
    namespace
    {
        GameObject* FindStageRoot(const SceneEditor& sceneEditor)
        {
            const PrefabEditorStage* stage = sceneEditor.GetPrefabStages().GetActiveStage();
            if (stage == nullptr || !stage->Asset || !stage->StageScene)
            {
                return nullptr;
            }

            const auto rootMap = stage->EditCloneMap.SourceToClonedGuid.find(stage->Asset->GetRootGuid());
            if (rootMap == stage->EditCloneMap.SourceToClonedGuid.end())
            {
                return nullptr;
            }

            if (!ObjectManager::HasInstance())
            {
                return nullptr;
            }

            return ObjectManager::Get().FindObjectAs<GameObject>(rootMap->second).get();
        }
    }

    bool PrefabEditConstraints::AllowAddTopLevelGameObject(const SceneEditor& sceneEditor, std::string* outError)
    {
        if (!sceneEditor.IsEditingPrefabStage())
        {
            return true;
        }

        if (outError)
        {
            *outError = "Prefab Stage cannot add a second top-level GameObject; select a parent first.";
        }
        return false;
    }

    bool PrefabEditConstraints::AllowDeleteGameObject(
        const SceneEditor& sceneEditor,
        uint64_t gameObjectId,
        std::string* outError)
    {
        if (!sceneEditor.IsEditingPrefabStage())
        {
            return true;
        }

        GameObject* root = FindStageRoot(sceneEditor);
        if (root != nullptr && root->GetID() == gameObjectId)
        {
            if (outError)
            {
                *outError = "Cannot delete Prefab Stage root GameObject.";
            }
            return false;
        }

        return true;
    }

    bool PrefabEditConstraints::AllowReparentToSceneRoot(
        const SceneEditor& sceneEditor,
        uint64_t gameObjectId,
        std::string* outError)
    {
        if (!sceneEditor.IsEditingPrefabStage())
        {
            return true;
        }

        GameObject* root = FindStageRoot(sceneEditor);
        if (root != nullptr && root->GetID() == gameObjectId)
        {
            // Root is already top-level; Detach is a no-op / harmless.
            return true;
        }

        if (outError)
        {
            *outError = "Cannot unparent GameObject to Stage root level (would create a second Prefab root).";
        }
        return false;
    }

    bool PrefabEditConstraints::AllowSaveAsScene(const SceneEditor& sceneEditor, std::string* outError)
    {
        if (!sceneEditor.IsEditingPrefabStage())
        {
            return true;
        }

        if (outError)
        {
            *outError = "Cannot Save Prefab Stage as a .mescene; use Save Prefab.";
        }
        return false;
    }

    bool PrefabEditConstraints::AllowEnterPlay(const SceneEditor& sceneEditor, std::string* outError)
    {
        if (!sceneEditor.IsEditingPrefabStage())
        {
            return true;
        }

        if (outError)
        {
            *outError = "Play / PIE is disabled while editing a Prefab document.";
        }
        return false;
    }

    bool PrefabEditConstraints::AllowCreatePrefab(const SceneEditor& sceneEditor, std::string* outError)
    {
        if (!sceneEditor.IsEditingPrefabStage())
        {
            return true;
        }

        if (outError)
        {
            *outError = "Create Prefab is only available in a Level Scene (not Prefab Stage).";
        }
        return false;
    }

    bool PrefabEditConstraints::AllowInstantiatePrefab(const SceneEditor& sceneEditor, std::string* outError)
    {
        if (!sceneEditor.IsEditingPrefabStage())
        {
            return true;
        }

        if (outError)
        {
            *outError = "Instantiate Prefab is only available in a Level Scene (not Prefab Stage).";
        }
        return false;
    }
}
