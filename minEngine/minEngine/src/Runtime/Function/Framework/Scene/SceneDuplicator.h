#pragma once

#include "Core.h"

#include <memory>

namespace minEngine
{
    class Scene;
    struct SceneCloneContext;

    class SceneDuplicator
    {
    public:
        static std::shared_ptr<Scene> DuplicateForPIE(const Scene& editorScene, SceneCloneContext& inOutContext);
        static void FinalizePIEScene(Scene& pieScene);
    };
}
