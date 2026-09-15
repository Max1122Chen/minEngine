#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Prefab/PrefabTypes.h"

#include <string>
#include <string_view>

namespace minEngine
{
    class MEObject;
    class Prefab;
    class Scene;

    class PrefabOverrideUtility
    {
    public:
        static bool TryRecordPropertyOverride(
            Scene& scene,
            MEObject& instanceObject,
            std::string_view propertyPath,
            std::string* outError = nullptr);

        static bool RevertProperty(
            Scene& scene,
            MEObject& instanceObject,
            std::string_view propertyPath,
            std::string* outError = nullptr);

        static bool RevertInstance(
            Scene& scene,
            const GUID& rootInstanceGuid,
            std::string* outError = nullptr);

        /** Propagate template defaults into one editor Scene (tests / explicit callers). */
        static void PropagateDefaultsToScene(const Prefab& prefab, Scene& scene);

        /** Propagate into SceneManager editor Scene when available. */
        static void PropagateDefaultsToOpenScenes(const Prefab& prefab);

        static bool HasOverride(
            const PrefabInstanceRecord& record,
            const GUID& templateObjectGuid,
            std::string_view propertyPath);

        static PrefabEditValidationResult ValidateEdit(
            Scene& scene,
            const PrefabInstanceRecord& record,
            const PrefabEditOp& op);
    };
}
