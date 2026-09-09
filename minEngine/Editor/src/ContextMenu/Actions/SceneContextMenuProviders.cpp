#include "ContextMenu/Actions/SceneContextMenuProviders.h"

#include "ContextMenu/EditorActionRegistry.h"
#include "ContextMenu/Contexts/SceneInspectorMenuContext.h"
#include "ContextMenu/EditorMenuBuilder.h"
#include "ContextMenu/EditorMenuContext.h"
#include "Services/AddComponentPicker.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "imgui.h"

#include <limits>

namespace minEngine
{
    class SceneAddComponentMenuProviderHost
    {
    public:
        static void Draw(IEditorContext& editor, const EditorMenuContext& ctx, EditorMenuBuilder& builder)
        {
            (void)builder;

            const uint64_t gameObjectId = GetInspectorGameObjectHeaderId(ctx);
            if (gameObjectId == kInvalidGameObjectId)
            {
                return;
            }

            SceneEditor* sceneEditor = GetSceneEditor(&editor);
            if (sceneEditor == nullptr)
            {
                return;
            }

            const bool menuOpen = ImGui::BeginMenu("Add Component##ContextMenu");
            if (!menuOpen)
            {
                s_SubMenuWasOpen = false;
                return;
            }

            if (!s_SubMenuWasOpen)
            {
                s_FilterBuffer[0] = '\0';
            }
            s_SubMenuWasOpen = true;

            AddComponentPicker::DrawContextSubMenu(
                editor, *sceneEditor, gameObjectId, s_FilterBuffer, sizeof(s_FilterBuffer));

            ImGui::EndMenu();
        }

    private:
        static constexpr uint64_t kInvalidGameObjectId = std::numeric_limits<uint64_t>::max();

        static uint64_t GetInspectorGameObjectHeaderId(const EditorMenuContext& ctx)
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

        static inline char s_FilterBuffer[128] = {};
        static inline bool s_SubMenuWasOpen = false;
    };

    void RegisterSceneContextMenuProviders(EditorActionRegistry& registry)
    {
        registry.RegisterProvider(&SceneAddComponentMenuProviderHost::Draw);
    }

} // namespace minEngine
