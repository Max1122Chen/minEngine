#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Prefab/PrefabTypes.h"

#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class GameObject;
    class Prefab;
    class Scene;

    struct ObjectCloneContext;

    class PrefabUtility
    {
    public:
        /** Prefab Stage editor-only objects (temp lights); excluded from Save / top-level checks. */
        static constexpr const char* kEditorTempStageObjectNamePrefix = "__ME_EditorTemp_";

        static std::shared_ptr<Prefab> CreatePrefabFromGameObject(
            GameObject& root,
            PrefabCreateReport* outReport = nullptr);

        static bool SavePrefabAsset(
            Prefab& prefab,
            const std::string& projectRelativePath,
            std::string* outError = nullptr);

        static std::shared_ptr<Prefab> LoadPrefabAsset(const GUID& assetGuid);

        static std::shared_ptr<GameObject> Instantiate(
            const Prefab& prefab,
            Scene& targetScene,
            const PrefabInstantiateParams& params,
            std::string* outError = nullptr);

        /** Same as Instantiate; optionally copies Source→clone map for Prefab Stage Save (strategy B). */
        static std::shared_ptr<GameObject> Instantiate(
            const Prefab& prefab,
            Scene& targetScene,
            const PrefabInstantiateParams& params,
            ObjectCloneContext* outCloneContext,
            std::string* outError = nullptr);

        static bool IsTemplateObject(const Prefab& prefab, const GUID& objectGuid);

        /**
         * Prefab Stage Save (strategy B): copy Stage tree into Prefab templates,
         * remapping Guids via inverted EditCloneMap (stage→template). Updates inOutEditMap.
         */
        static bool WriteStageTreeToPrefab(
            Scene& stageScene,
            Prefab& prefab,
            ObjectCloneContext& inOutEditMap,
            std::string* outError = nullptr);

        static PrefabInstanceRecord* FindInstanceRecord(Scene& scene, const GUID& instanceObjectGuid);
        static const PrefabInstanceRecord* FindInstanceRecord(const Scene& scene, const GUID& instanceObjectGuid);

        static bool IsEditorTempStageObject(const GameObject& gameObject);

        static std::vector<PrefabInstanceRef> FindInstanceRefsInScene(
            Scene& scene,
            const GUID& prefabAssetGuid);

        /** Scans SceneManager editor Scene only (open Level); does not scan disk .mescene files. */
        static std::vector<PrefabInstanceRef> FindInstanceRefsInOpenEditorScenes(const GUID& prefabAssetGuid);

        /** Remove PrefabInstanceRecord; keep GameObject tree. */
        static bool UnpackInstance(Scene& scene, const GUID& rootInstanceGuid, std::string* outError = nullptr);

        static bool UnpackAllInstancesOfPrefab(
            Scene& scene,
            const GUID& prefabAssetGuid,
            size_t* outUnpackedCount = nullptr,
            std::string* outError = nullptr);

        static std::string FormatPrefabInstanceRefsMessage(
            const std::string& assetPath,
            const std::vector<PrefabInstanceRef>& refs);
    };
}
