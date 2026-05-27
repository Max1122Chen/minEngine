#include "ContextMenu/EditorMenuBuilder.h"

#include "ContextMenu/EditorActionRegistry.h"
#include "ContextMenu/IEditorAction.h"
#include "Shell/IEditorContext.h"

#include "imgui.h"

namespace minEngine
{
    EditorMenuBuilder::EditorMenuBuilder(EditorActionRegistry& registry)
        : m_Registry(registry)
    {
    }

    void EditorMenuBuilder::Draw(IEditorContext& editor, const EditorMenuContext& ctx)
    {
        const std::vector<const IEditorAction*> actions = m_Registry.Query(ctx);
        if (!actions.empty())
        {
            EditorMenuSectionId currentSection = actions.front()->GetSection();
            bool sectionStarted = false;

            for (const IEditorAction* action : actions)
            {
                if (!action)
                {
                    continue;
                }

                if (sectionStarted && action->GetSection() != currentSection)
                {
                    ImGui::Separator();
                    currentSection = action->GetSection();
                }

                const bool enabled = action->CanExecute(ctx);
                DrawAction(editor, *action, ctx, enabled);
                sectionStarted = true;
            }
        }

        m_Registry.InvokeProviders(editor, ctx, *this);
    }

    void EditorMenuBuilder::DrawAction(
        IEditorContext& editor,
        const IEditorAction& action,
        const EditorMenuContext& ctx,
        bool enabled)
    {
        if (!enabled)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::MenuItem(action.GetLabel(ctx)))
        {
            action.Execute(editor, ctx);
        }

        if (!enabled)
        {
            const char* disabledReason = action.GetDisabledReason(ctx);
            if (disabledReason && disabledReason[0] != '\0')
            {
                ImGui::SetItemTooltip("%s", disabledReason);
            }
            ImGui::EndDisabled();
        }
    }

} // namespace minEngine
