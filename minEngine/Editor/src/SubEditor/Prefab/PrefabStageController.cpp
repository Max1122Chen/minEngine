#include "SubEditor/Prefab/PrefabStageController.h"

#include "Shell/IEditorContext.h"
#include "Shell/ViewportClientRegistry.h"
#include "SubEditor/Scene/SceneEditingViewportClient.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
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
    namespace
    {
        constexpr const char* kTempStageLightObjectName = "__ME_EditorTemp_DirectionalLight";

        SceneEditingViewportClient* TryGetSceneEditingViewport(IEditorContext& context)
        {
            if (SceneEditor* sceneEditor = GetSceneEditor(&context))
            {
                if (SceneEditingViewportClient* client = sceneEditor->TryGetSceneEditingViewportClient())
                {
                    return client;
                }
            }

            return context.GetViewportRegistry().FindSceneEditingViewportClient("scene_editing_viewport");
        }

        void StashAndApplyCameraPose(
            IEditorContext& context,
            EditorFlyCameraPose& stashTarget,
            const EditorFlyCameraPose& poseToApply,
            bool bUseDefaultIfInvalid)
        {
            if (SceneEditingViewportClient* viewport = TryGetSceneEditingViewport(context))
            {
                viewport->CaptureFlyCameraPose(stashTarget);
                stashTarget.bValid = true;

                EditorFlyCameraPose applyPose = poseToApply;
                if (!applyPose.bValid)
                {
                    if (!bUseDefaultIfInvalid)
                    {
                        return;
                    }
                    applyPose = SceneEditingViewportClient::MakeDefaultPrefabStageCameraPose();
                }
                viewport->ApplyFlyCameraPose(applyPose);
            }
        }

        bool EnsureTempStageDirectionalLight(Scene& stageScene)
        {
            for (const std::shared_ptr<GameObject>& gameObject : stageScene.GetAllGameObjects())
            {
                if (gameObject && PrefabUtility::IsEditorTempStageObject(*gameObject)
                    && gameObject->GetName() == kTempStageLightObjectName)
                {
                    return true;
                }
            }

            std::shared_ptr<GameObject> lightObject = stageScene.CreateGameObject();
            if (!lightObject)
            {
                return false;
            }

            lightObject->Rename(kTempStageLightObjectName);
            auto lightComponent = lightObject->AddComponent<DirectionalLightComponent>();
            if (!lightComponent)
            {
                return false;
            }

            lightObject->SetRootComponent(lightComponent.get());
            lightComponent->SetLightColor(LinearColor(1.0f, 0.98f, 0.95f, 1.0f));
            lightComponent->SetIntensity(1.2f);
            lightComponent->SetDiffuseFactor(12.0f);
            lightObject->SetRotationEulerDegrees(Vector3(-52.0f, 132.0f, 0.0f));
            return true;
        }
    }

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

        outStage.bHasTempStageLight = EnsureTempStageDirectionalLight(*stageScene);
        if (!outStage.bHasTempStageLight)
        {
            ME_LOG(LogEditor, Warn, "PrefabStageController::BuildStage: failed to add temp Stage light.");
        }

        outStage.AssetKey = meta.AssetPath;
        outStage.Asset = std::move(prefab);
        outStage.StageScene = std::move(stageScene);
        outStage.EditCloneMap = std::move(editMap);
        outStage.CameraPose = SceneEditingViewportClient::MakeDefaultPrefabStageCameraPose();
        outStage.bDirty = false;
        return true;
    }

    void PrefabStageController::DestroyStage(PrefabEditorStage& stage)
    {
        stage.EditCloneMap.Clear();
        stage.StageScene.reset();
        stage.Asset.reset();
        stage.CameraPose = {};
        stage.bHasTempStageLight = false;
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

        SceneEditor* sceneEditor = GetSceneEditor(&context);
        const std::string previousKey = m_ActiveAssetKey;

        if (sceneEditor != nullptr)
        {
            if (previousKey.empty())
            {
                StashAndApplyCameraPose(
                    context, sceneEditor->GetLevelCameraPoseMutable(), stage->CameraPose, true);
            }
            else if (previousKey != assetKey)
            {
                if (PrefabEditorStage* previousStage = FindStage(previousKey))
                {
                    StashAndApplyCameraPose(context, previousStage->CameraPose, stage->CameraPose, true);
                }
                else
                {
                    StashAndApplyCameraPose(
                        context, sceneEditor->GetLevelCameraPoseMutable(), stage->CameraPose, true);
                }
            }
        }

        m_ActiveAssetKey = assetKey;
        context.SetInspectingScene(stage->StageScene.get());
        if (sceneEditor != nullptr)
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

        PrefabEditorStage* activeStage = FindStage(m_ActiveAssetKey);
        SceneEditor* sceneEditor = GetSceneEditor(&context);
        if (activeStage != nullptr && sceneEditor != nullptr)
        {
            StashAndApplyCameraPose(
                context, activeStage->CameraPose, sceneEditor->GetLevelCameraPose(), false);
        }

        m_ActiveAssetKey.clear();

        Scene* documentScene = nullptr;
        if (sceneEditor != nullptr)
        {
            documentScene = sceneEditor->GetDocumentScene();
        }
        context.SetInspectingScene(documentScene);
        if (sceneEditor != nullptr)
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
        const std::vector<std::shared_ptr<GameObject>> previousTemplates = stage->Asset->GetTemplateObjects();
        const GUID previousRootGuid = stage->Asset->GetRootGuid();
        ObjectCloneContext previousEditMap = stage->EditCloneMap;

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
            PrefabUtility::RestoreTemplateObjects(*stage->Asset, previousTemplates, previousRootGuid);
            stage->EditCloneMap = std::move(previousEditMap);
            if (outError)
            {
                *outError = saveError.empty() ? "SavePrefabAsset failed." : saveError;
            }
            return false;
        }

        if (PrefabOverrideUtility::PropagateDefaultsToEditorScene(*stage->Asset))
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
