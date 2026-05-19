#pragma once
#include "Core.h"

namespace minEngine
{
    class Scene;

    void PopulateEditorDefaultScene(Scene& scene);

    // Creates MaterialIRSmoke scene, populates it, and sets it as the active scene.
    bool SetEditorMaterialIRSmokeActiveScene();
}
