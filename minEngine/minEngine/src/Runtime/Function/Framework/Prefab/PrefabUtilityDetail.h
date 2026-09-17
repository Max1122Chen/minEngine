#pragma once

#include "Core.h"
#include "Runtime/Core/Object/ObjectCloneContext.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Prefab/PrefabTypes.h"

#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class GameObject;
    class Prefab;
    class Scene;

    /**
     * Shared helpers for PrefabUtility translation units (not a public API).
     * Class-scoped statics (not anonymous-namespace free functions) per project C++ style.
     */
    class PrefabUtilityDetail
    {
    public:
        enum class EMappingDirection : uint8_t
        {
            /** Instantiate: Source(template) → Instance */
            TemplateToInstance = 0,
            /** Create: Source(scene) → Template; record stores Template←Instance inverted */
            SceneToTemplateInverted,
        };

        static constexpr Serialization::SerializerOptions kPrefabCloneOptions{
            .enumAsString = true,
            .strictTypeCheck = true,
            .skipUnknownField = false,
        };

        static constexpr Serialization::SerializerOptions kPrefabDiskOptions{
            .enumAsString = true,
            .strictTypeCheck = false,
            .skipUnknownField = true,
        };

        static void CollectSubtree(GameObject& root, std::vector<GameObject*>& outOrdered);

        static std::shared_ptr<GameObject> CloneGameObjectAsTemplate(
            GameObject& source,
            ObjectCloneContext& cloneContext,
            std::vector<Serialization::PendingObjectRef>& unresolvedRefs,
            std::string* outError);

        static void BuildInstanceRecordFromCloneContext(
            PrefabInstanceRecord& record,
            const GUID& prefabAssetGuid,
            const GUID& rootInstanceGuid,
            const ObjectCloneContext& cloneContext,
            EMappingDirection direction);

        static void ClearUnresolvedPendingRefs(
            std::vector<Serialization::PendingObjectRef>& unresolvedRefs,
            PrefabCreateReport* outReport);

        static void FinalizeSceneObjects(Scene& scene);

        static void RestorePreviousTemplates(
            Prefab& prefab,
            const std::vector<std::shared_ptr<GameObject>>& previousTemplates,
            const GUID& previousRootGuid);

        static void RefreshEditCloneMapAfterWrite(
            Scene& stageScene,
            const ObjectCloneContext& writeContext,
            ObjectCloneContext& inOutEditMap);
    };
}
