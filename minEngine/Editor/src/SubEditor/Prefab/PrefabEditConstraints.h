#pragma once

#include "Core.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    class SceneEditor;

    /** Prefab Stage edit policy (ED-F16): single root, no Save-As Scene, no delete root. */
    class PrefabEditConstraints
    {
    public:
        static bool AllowAddTopLevelGameObject(const SceneEditor& sceneEditor, std::string* outError = nullptr);
        static bool AllowDeleteGameObject(
            const SceneEditor& sceneEditor,
            uint64_t gameObjectId,
            std::string* outError = nullptr);
        static bool AllowReparentToSceneRoot(
            const SceneEditor& sceneEditor,
            uint64_t gameObjectId,
            std::string* outError = nullptr);
        static bool AllowReparentGameObject(
            const SceneEditor& sceneEditor,
            uint64_t gameObjectId,
            uint64_t newParentId,
            std::string* outError = nullptr);
        static bool AllowSaveAsScene(const SceneEditor& sceneEditor, std::string* outError = nullptr);
        static bool AllowEnterPlay(const SceneEditor& sceneEditor, std::string* outError = nullptr);
        static bool AllowCreatePrefab(const SceneEditor& sceneEditor, std::string* outError = nullptr);
        static bool AllowInstantiatePrefab(const SceneEditor& sceneEditor, std::string* outError = nullptr);
    };
}
