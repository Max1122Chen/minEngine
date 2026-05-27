#pragma once

#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "Shell/IEditorContext.h"

namespace minEngine
{
    inline SceneEditor* GetSceneEditor(IEditorContext* context)
    {
        if (!context)
        {
            return nullptr;
        }
        return dynamic_cast<SceneEditor*>(context->FindSubModule(SceneEditor::kModuleId));
    }

    inline const SceneEditor* GetSceneEditor(const IEditorContext* context)
    {
        if (!context)
        {
            return nullptr;
        }
        return dynamic_cast<const SceneEditor*>(context->FindSubModule(SceneEditor::kModuleId));
    }

    inline MaterialEditor* GetMaterialEditor(IEditorContext* context)
    {
        if (!context)
        {
            return nullptr;
        }
        return dynamic_cast<MaterialEditor*>(context->FindSubModule(MaterialEditor::kModuleId));
    }

    inline const MaterialEditor* GetMaterialEditor(const IEditorContext* context)
    {
        if (!context)
        {
            return nullptr;
        }
        return dynamic_cast<const MaterialEditor*>(context->FindSubModule(MaterialEditor::kModuleId));
    }

    inline MaterialEditor* GetMaterialEditorFromContext(IEditorContext* context)
    {
        return GetMaterialEditor(context);
    }
}
