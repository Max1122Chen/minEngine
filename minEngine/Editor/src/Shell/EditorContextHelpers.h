#pragma once

#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"
#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "SubEditor/Sprite/SpriteEditor.h"
#include "SubEditor/SpriteAtlas/SpriteAtlasEditor.h"
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

    inline SpriteEditor* GetSpriteEditor(IEditorContext* context)
    {
        if (!context)
        {
            return nullptr;
        }
        return dynamic_cast<SpriteEditor*>(context->FindSubModule(SpriteEditor::kModuleId));
    }

    inline const SpriteEditor* GetSpriteEditor(const IEditorContext* context)
    {
        if (!context)
        {
            return nullptr;
        }
        return dynamic_cast<const SpriteEditor*>(context->FindSubModule(SpriteEditor::kModuleId));
    }

    inline SpriteEditor* GetSpriteEditor(IEditorContext& context)
    {
        return GetSpriteEditor(&context);
    }

    inline SpriteAtlasEditor* GetSpriteAtlasEditor(IEditorContext* context)
    {
        if (!context)
        {
            return nullptr;
        }
        return dynamic_cast<SpriteAtlasEditor*>(context->FindSubModule(SpriteAtlasEditor::kModuleId));
    }

    inline const SpriteAtlasEditor* GetSpriteAtlasEditor(const IEditorContext* context)
    {
        if (!context)
        {
            return nullptr;
        }
        return dynamic_cast<const SpriteAtlasEditor*>(
            context->FindSubModule(SpriteAtlasEditor::kModuleId));
    }

    inline SpriteAtlasEditor* GetSpriteAtlasEditor(IEditorContext& context)
    {
        return GetSpriteAtlasEditor(&context);
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

    inline EditorDocumentSession* GetSpriteDocumentSession(IEditorContext& context)
    {
        SpriteEditor* editor = GetSpriteEditor(&context);
        if (editor == nullptr || !editor->GetSession().HasOpenSprite())
        {
            return nullptr;
        }
        return FindDocumentSessionByTypeAndKey(context, "Sprite", editor->GetSession().AssetPath);
    }

    inline EditorDocumentSession* GetSpriteAtlasDocumentSession(IEditorContext& context)
    {
        SpriteAtlasEditor* editor = GetSpriteAtlasEditor(&context);
        if (editor == nullptr || !editor->GetSession().HasOpenAtlas())
        {
            return nullptr;
        }
        return FindDocumentSessionByTypeAndKey(context, "SpriteAtlas", editor->GetSession().AssetPath);
    }
}
