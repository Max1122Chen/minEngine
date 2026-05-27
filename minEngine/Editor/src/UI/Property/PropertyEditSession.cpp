#include "UI/Property/PropertyEditSession.h"

#include "SubEditor/Scene/SceneEditor.h"

namespace minEngine
{
    PropertyEditSession PropertyEditSession::ForSceneEditor(SceneEditor& sceneEditor)
    {
        PropertyEditSession session;
        session.ContextKind = EditorPropertyEditContextKind::SceneInstance;
        session.OnMarkDirty = [&sceneEditor]() { sceneEditor.MarkSceneDirty(); };
        return session;
    }

    PropertyEditSession PropertyEditSession::ForAssetDefaults()
    {
        PropertyEditSession session;
        session.ContextKind = EditorPropertyEditContextKind::AssetDefaults;
        return session;
    }
}
