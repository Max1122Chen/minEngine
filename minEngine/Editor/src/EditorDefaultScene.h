#pragma once
#include "Core.h"

namespace minEngine
{
    class Scene;

    // Reserved hook for editor-authored default scene setup. Intentionally empty:
    // startup loads scenes from project assets via OpenProject / EditorDefaultSceneName.
    void PopulateEditorDefaultScene(Scene& scene);
}
