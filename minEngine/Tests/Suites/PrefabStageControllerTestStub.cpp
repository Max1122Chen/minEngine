// Linked into minEngineTests so AssetEditorServices::Delete can resolve Prefab stage queries
// without pulling the full SceneEditor / Prefab submodule graph.
#include "SubEditor/Prefab/PrefabStageController.h"

namespace minEngine
{
    bool PrefabStageController::HasStage(const std::string& /*assetPath*/) const
    {
        return false;
    }
}
