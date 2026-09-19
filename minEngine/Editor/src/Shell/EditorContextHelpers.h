#pragma once

#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"
#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "Shell/Document/EditorDocumentHost.h"
#include "Shell/Document/EditorDocumentSession.h"
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

    inline AnimationGraphEditor* GetAnimationGraphEditor(IEditorContext* context)
    {
        if (!context)
        {
            return nullptr;
        }
        return dynamic_cast<AnimationGraphEditor*>(
            context->FindSubModule(AnimationGraphEditor::kModuleId));
    }

    inline const AnimationGraphEditor* GetAnimationGraphEditor(const IEditorContext* context)
    {
        if (!context)
        {
            return nullptr;
        }
        return dynamic_cast<const AnimationGraphEditor*>(
            context->FindSubModule(AnimationGraphEditor::kModuleId));
    }

    /** Scene document session for command stack (Active Scene tab, else first Scene session). */
    inline EditorDocumentSession* GetSceneDocumentSession(IEditorContext& context)
    {
        EditorDocumentHost& host = context.GetDocumentHost();
        if (EditorDocumentSession* active = host.GetActiveSession())
        {
            if (active->GetTypeId() == "Scene")
            {
                return active;
            }
        }

        for (const auto& sessionPtr : host.GetSessions())
        {
            if (sessionPtr && sessionPtr->GetTypeId() == "Scene")
            {
                return sessionPtr.get();
            }
        }
        return nullptr;
    }

    inline EditorDocumentSession* FindDocumentSessionByTypeAndKey(
        IEditorContext& context,
        std::string_view typeId,
        std::string_view assetKey)
    {
        EditorDocumentHost& host = context.GetDocumentHost();
        if (EditorDocumentSession* active = host.GetActiveSession())
        {
            if (active->GetTypeId() == typeId && active->GetAssetKey() == assetKey)
            {
                return active;
            }
        }

        for (const auto& sessionPtr : host.GetSessions())
        {
            if (sessionPtr && sessionPtr->GetTypeId() == typeId && sessionPtr->GetAssetKey() == assetKey)
            {
                return sessionPtr.get();
            }
        }
        return nullptr;
    }

    inline EditorDocumentSession* GetMaterialDocumentSession(IEditorContext& context)
    {
        MaterialEditor* editor = GetMaterialEditor(&context);
        if (editor == nullptr || !editor->GetSession().HasOpenMaterial())
        {
            return nullptr;
        }
        return FindDocumentSessionByTypeAndKey(context, "Material", editor->GetSession().AssetPath);
    }

    inline EditorDocumentSession* GetAnimationGraphDocumentSession(IEditorContext& context)
    {
        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr || !editor->GetSession().HasOpenGraph())
        {
            return nullptr;
        }
        return FindDocumentSessionByTypeAndKey(context, "AnimationGraph", editor->GetSession().AssetPath);
    }
}
