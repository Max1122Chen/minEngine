#include "SceneDuplicator.h"

#include "Scene.h"
#include "SceneCloneContext.h"
#include "SceneManager.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/Serializer.h"

namespace minEngine
{
    std::shared_ptr<Scene> SceneDuplicator::DuplicateForPIE(const Scene& editorScene, SceneCloneContext& inOutContext)
    {
        std::vector<uint8_t> buffer;
        std::vector<Serialization::PendingObjectRef> unresolvedRefs;
        const Serialization::SerializeResult serializeResult = Serialization::Serializer::SerializeObjectToBuffer(
            "minEngine::Scene",
            &editorScene,
            buffer);
        if (!serializeResult.ok)
        {
            ME_CORE_ERROR("SceneDuplicator: failed to serialize editor scene '{}'.", editorScene.GetSceneName());
            return nullptr;
        }

        std::shared_ptr<Scene> pieScene = NewObject<Scene>();
        pieScene->m_SceneName = editorScene.GetSceneName() + "_PIE";
        pieScene->SetSceneType(ESceneType::PIE);
        pieScene->SetTickPolicy(ESceneTickPolicy::Gameplay);

        if (ObjectManager::HasInstance())
        {
            ObjectManager::Get().UnregisterObject(pieScene.get());
        }

        inOutContext.SourceToClonedGuid.clear();
        inOutContext.ClonedBySourceGuid.clear();

        Serialization::Serializer::SetActiveCloneContext(&inOutContext);
        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::DeserializeObjectFromBuffer(
            "minEngine::Scene",
            pieScene.get(),
            buffer,
            unresolvedRefs);
        Serialization::Serializer::SetActiveCloneContext(nullptr);

        if (!deserializeResult.ok)
        {
            ME_CORE_ERROR("SceneDuplicator: failed to deserialize PIE scene from editor scene '{}'.", editorScene.GetSceneName());
            return nullptr;
        }

        Serialization::Serializer::SetActiveCloneContext(&inOutContext);
        const Serialization::SerializeResult resolveResult = Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
        Serialization::Serializer::SetActiveCloneContext(nullptr);

        if (!resolveResult.ok)
        {
            ME_CORE_ERROR("SceneDuplicator: failed to resolve pending refs for PIE scene '{}'.", editorScene.GetSceneName());
            return nullptr;
        }

        const GUID sourceSceneGuid = editorScene.GetGuid();
        const GUID newSceneGuid = GenerateGUID();
        inOutContext.RecordClone(sourceSceneGuid, pieScene, newSceneGuid);
        if (ObjectManager::HasInstance())
        {
            ObjectManager::Get().RemapObjectGuid(pieScene, newSceneGuid);
        }

        FinalizePIEScene(*pieScene);
        return pieScene;
    }

    void SceneDuplicator::FinalizePIEScene(Scene& pieScene)
    {
        pieScene.RebuildRuntimeGameObjectIndex();
        pieScene.EnsureRenderScene();
        SceneManager::RebuildSceneComponentAttachHierarchy(&pieScene);
        if (SceneManager::HasInstance())
        {
            SceneManager::Get().ResolvePendingActivationsForScene(&pieScene);
        }
    }
}
