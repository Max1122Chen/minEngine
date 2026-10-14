#include "Subsystem.h"

#include "Runtime/Function/Framework/Scene/Scene.h"

namespace minEngine
{
    Scene* WorldSubsystem::GetScene() const
    {
        MEObject* outer = const_cast<MEObject*>(GetOuter());
        if (outer == nullptr || !outer->IsA(Scene::StaticClass()))
        {
            return nullptr;
        }
        return static_cast<Scene*>(outer);
    }

    bool WorldSubsystem::ShouldCreateSubsystem(MEObject* outer) const
    {
        if (outer == nullptr || !outer->IsA(Scene::StaticClass()))
        {
            return false;
        }

        const Scene* scene = static_cast<const Scene*>(outer);
        return DoesSupportWorldType(scene->GetSceneType());
    }
}
