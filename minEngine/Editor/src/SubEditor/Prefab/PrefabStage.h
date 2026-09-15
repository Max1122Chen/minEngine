#pragma once

#include "Runtime/Core/Object/ObjectCloneContext.h"

#include <memory>
#include <string>

namespace minEngine
{
    class Prefab;
    class Scene;

    /** Transient Prefab editing stage (not a project .mescene). */
    struct PrefabEditorStage
    {
        std::string AssetKey;
        std::shared_ptr<Prefab> Asset;
        std::shared_ptr<Scene> StageScene;
        ObjectCloneContext EditCloneMap;
        bool bDirty = false;
    };
}
