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

        /** Propagate template defaults into one editor Scene. Returns true if any property was written. */
        static bool PropagateDefaultsToScene(const Prefab& prefab, Scene& scene);

        /** Propagate into SceneManager editor Scene when available. Returns true if any property was written. */
        static bool PropagateDefaultsToEditorScene(const Prefab& prefab);

        /** @deprecated Prefer PropagateDefaultsToEditorScene — same behavior. */
        static bool PropagateDefaultsToOpenScenes(const Prefab& prefab)
        {
            return PropagateDefaultsToEditorScene(prefab);
        }

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
