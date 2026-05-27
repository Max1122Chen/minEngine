#include "ContextMenu/Actions/SceneContextMenuProviders.h"

#include "ContextMenu/EditorActionRegistry.h"
#include "ContextMenu/Contexts/SceneInspectorMenuContext.h"
#include "ContextMenu/EditorMenuBuilder.h"
#include "ContextMenu/EditorMenuContext.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "imgui.h"

#include <limits>
#include <string>

namespace minEngine
{
    namespace
    {
        constexpr uint64_t kInvalidGameObjectId = std::numeric_limits<uint64_t>::max();

        uint64_t GetInspectorGameObjectHeaderId(const EditorMenuContext& ctx)
        {
            if (const SceneInspectorMenuContext* inspectorCtx = ctx.Find<SceneInspectorMenuContext>())
            {
                if (inspectorCtx->SelectionKind == SceneInspectorSelectionKind::GameObjectHeader)
                {
                    return inspectorCtx->GameObjectId;
                }
            }

            return kInvalidGameObjectId;
        }

        std::string GetShortComponentTypeName(const std::string& fullTypeName)
        {
            const size_t scopePos = fullTypeName.rfind("::");
            if (scopePos == std::string::npos)
            {
                return fullTypeName;
            }

            return fullTypeName.substr(scopePos + 2);
        }

        void SceneAddComponentMenuProvider(
            IEditorContext& editor,
            const EditorMenuContext& ctx,
            EditorMenuBuilder& builder)
        {
            const uint64_t gameObjectId = GetInspectorGameObjectHeaderId(ctx);
            if (gameObjectId == kInvalidGameObjectId)
            {
                return;
            }

            SceneEditor* sceneEditor = GetSceneEditor(&editor);
            if (!sceneEditor)
            {
                return;
            }

            const std::vector<std::string>& componentTypeNames = sceneEditor->GetAllComponentTypeNames();
            if (componentTypeNames.empty())
            {
                return;
            }

            if (!builder.EnsureSectionOpen(EditorMenuSectionId::Create))
            {
                return;
            }

            if (!ImGui::BeginMenu("Add Component##ContextMenu"))
            {
                return;
            }

            for (const std::string& typeName : componentTypeNames)
            {
                ImGui::PushID(typeName.c_str());
                const std::string displayName = GetShortComponentTypeName(typeName);
                const std::string menuItemLabel = displayName + "##" + typeName;
                builder.DrawProviderMenuItem(
                    editor,
                    ctx,
                    menuItemLabel.c_str(),
                    true,
                    "",
                    [sceneEditor, gameObjectId, typeName](IEditorContext& ed, const EditorMenuContext& menuCtx)
                    {
                        (void)menuCtx;
                        sceneEditor->SelectGameObject(gameObjectId);
                        sceneEditor->SubmitAddComponentToSelectedGameObject(ed, typeName);
                    });
                ImGui::PopID();
            }

            ImGui::EndMenu();
        }
    }

    void RegisterSceneContextMenuProviders(EditorActionRegistry& registry)
    {
        registry.RegisterProvider(SceneAddComponentMenuProvider);
    }

} // namespace minEngine
