#pragma once

#include "UI/Property/PropertyEditTypes.h"

#include <functional>

namespace minEngine
{
    class SceneEditor;

    /** Per-drawer callbacks for property edits (Undo wiring stays in Inspector sources for now). */
    struct PropertyEditSession
    {
        EditorPropertyEditContextKind ContextKind = EditorPropertyEditContextKind::SceneInstance;
        std::function<void()> OnMarkDirty;

        void MarkDirty() const
        {
            if (OnMarkDirty)
            {
                OnMarkDirty();
            }
        }

        static PropertyEditSession ForSceneEditor(SceneEditor& sceneEditor);
        static PropertyEditSession ForAssetDefaults();
    };
}
