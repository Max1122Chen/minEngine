#include "SubEditor/Prefab/PrefabStageController.h"

#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Prefab/PrefabOverrideUtility.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include "Shell/EditorContextHelpers.h"

namespace minEngine
{
    bool PrefabStageController::BuildStage(const AssetMeta& meta, PrefabEditorStage& outStage, std::string* outError)
    {
        if (!AssetManager::HasInstance())
        {
            if (outError)
            {
                *outError = "AssetManager is not available.";
            }
            return false;
        }

        std::shared_ptr<Prefab> prefab = AssetManager::Get().LoadAsset<Prefab>(meta.AssetPath);
        if (!prefab)
        {
            if (outError)
            {
                *outError = "Failed to load Prefab asset: " + meta.AssetPath;
            }
            return false;
        }

        std::shared_ptr<Scene> stageScene = NewObject<Scene>("PrefabStage");
        stageScene->SetSceneName(meta.AssetName.empty() ? "PrefabStage" : meta.AssetName);
        stageScene->EnsureRenderScene();
        stageScene->SetSceneType(ESceneType::Editor);
        stageScene->SetTickPolicy(ESceneTickPolicy::ViewportOnly);

        PrefabInstantiateParams params;
        params.bRegisterPrefabInstance = false;

        ObjectCloneContext editMap;
        std::string instantiateError;
        std::shared_ptr<GameObject> root =
            PrefabUtility::Instantiate(*prefab, *stageScene, params, &editMap, &instantiateError);
        if (!root)
        {
            if (outError)
            {
                *outError = instantiateError.empty() ? "Prefab Instantiate into Stage failed." : instantiateError;
            }
            return false;
        }

        outStage.AssetKey = meta.AssetPath;
        outStage.Asset = std::move(prefab);
        outStage.StageScene = std::move(stageScene);
        outStage.EditCloneMap = std::move(editMap);
        outStage.bDirty = false;
        return true;
    }

    void PrefabStageController::DestroyStage(PrefabEditorStage& stage)
    {
        stage.EditCloneMap.Clear();
        stage.StageScene.reset();
        stage.Asset.reset();
        stage.bDirty = false;
        stage.AssetKey.clear();
    }

    bool PrefabStageController::OpenFromAsset(const AssetMeta& meta, std::string* outError)
    {
        if (meta.AssetPath.empty() || meta.AssetType != "Prefab")
        {
            if (outError)
            {
                *outError = "Asset is not a Prefab.";
            }
            return false;
        }

        if (FindStage(meta.AssetPath) != nullptr)
        {
            return true;
        }

        PrefabEditorStage stage;
        if (!BuildStage(meta, stage, outError))
        {
            return false;
        }

        m_StagesByKey[meta.AssetPath] = std::move(stage);
        return true;
    }

    bool PrefabStageController::Activate(const std::string& assetKey, IEditorContext& context)
    {
        PrefabEditorStage* stage = FindStage(assetKey);
        if (stage == nullptr || !stage->StageScene)
        {
            ME_LOG(LogEditor, Error, "PrefabStageController::Activate: missing stage '{}'.", assetKey);
            return false;
        }

        m_ActiveAssetKey = assetKey;
        context.SetInspectingScene(stage->StageScene.get());
        if (SceneEditor* sceneEditor = GetSceneEditor(&context))
        {
            sceneEditor->ClearSelectedGameObject();
            sceneEditor->SyncSelectionWithScene();
        }
        return true;
    }

    void PrefabStageController::Discard(const std::string& assetKey)
    {
        const auto it = m_StagesByKey.find(assetKey);
        if (it == m_StagesByKey.end())
        {
            return;
        }

        if (m_ActiveAssetKey == assetKey)
        {
            m_ActiveAssetKey.clear();
        }

        DestroyStage(it->second);
        m_StagesByKey.erase(it);
    }

    void PrefabStageController::ExitActive(IEditorContext& context)
    {
        if (m_ActiveAssetKey.empty())
        {
            return;
        }

        m_ActiveAssetKey.clear();

        Scene* documentScene = nullptr;
        if (SceneEditor* sceneEditor = GetSceneEditor(&context))
        {
            documentScene = sceneEditor->GetDocumentScene();
        }
        context.SetInspectingScene(documentScene);
        if (SceneEditor* sceneEditor = GetSceneEditor(&context))
        {
            sceneEditor->ClearSelectedGameObject();
            sceneEditor->SyncSelectionWithScene();
        }
    }

    bool PrefabStageController::HasStage(const std::string& assetKey) const
    {
        return FindStage(assetKey) != nullptr;
    }

    bool PrefabStageController::IsDirty(const std::string& assetKey) const
    {
        const PrefabEditorStage* stage = FindStage(assetKey);
        return stage != nullptr && stage->bDirty;
    }

    void PrefabStageController::MarkDirty(const std::string& assetKey)
    {
        if (PrefabEditorStage* stage = FindStage(assetKey))
        {
            stage->bDirty = true;
        }
    }

    void PrefabStageController::ClearDirty(const std::string& assetKey)
    {
        if (PrefabEditorStage* stage = FindStage(assetKey))
        {
            stage->bDirty = false;
        }
    }

    PrefabEditorStage* PrefabStageController::FindStage(const std::string& assetKey)
    {
        const auto it = m_StagesByKey.find(assetKey);
        return it != m_StagesByKey.end() ? &it->second : nullptr;
    }

    const PrefabEditorStage* PrefabStageController::FindStage(const std::string& assetKey) const
    {
        const auto it = m_StagesByKey.find(assetKey);
        return it != m_StagesByKey.end() ? &it->second : nullptr;
    }

    PrefabEditorStage* PrefabStageController::GetActiveStage()
    {
        return FindStage(m_ActiveAssetKey);
    }

    const PrefabEditorStage* PrefabStageController::GetActiveStage() const
    {
        return FindStage(m_ActiveAssetKey);
    }

    bool PrefabStageController::SaveActive(IEditorContext& context, std::string* outError)
    {
        PrefabEditorStage* stage = GetActiveStage();
        if (stage == nullptr || !stage->Asset || !stage->StageScene)
        {
            if (outError)
            {
                *outError = "No active Prefab Stage to save.";
            }
            return false;
        }

        std::string writeError;
        if (!PrefabUtility::WriteStageTreeToPrefab(
                *stage->StageScene,
                *stage->Asset,
                stage->EditCloneMap,
                &writeError))
        {
            if (outError)
            {
                *outError = writeError;
            }
            return false;
        }

        std::string saveError;
        if (!PrefabUtility::SavePrefabAsset(*stage->Asset, stage->AssetKey, &saveError))
        {
            if (outError)
            {
                *outError = saveError.empty() ? "SavePrefabAsset failed." : saveError;
            }
            return false;
        }

        if (PrefabOverrideUtility::PropagateDefaultsToOpenScenes(*stage->Asset))
        {
            if (SceneEditor* sceneEditor = GetSceneEditor(&context))
            {
                sceneEditor->MarkDocumentSceneDirty();
            }
        }
        stage->bDirty = false;
        (void)context;
        ME_LOG(LogEditor, Info, "Prefab Stage saved '{}'.", stage->AssetKey);
        return true;
    }
}
