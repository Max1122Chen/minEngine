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
        m_IsProviderCreateMenuOpen = false;

        DrawStaticActions(editor, ctx);
        m_Registry.InvokeProviders(editor, ctx, *this);

        if (m_IsProviderCreateMenuOpen)
        {
            ImGui::EndMenu();
            m_IsProviderCreateMenuOpen = false;
        }
    }

    void EditorMenuBuilder::DrawStaticActions(IEditorContext& editor, const EditorMenuContext& ctx)
    {
        const std::vector<const IEditorAction*> actions = m_Registry.Query(ctx);
        if (actions.empty())
        {
            return;
        }

        bool hasRenderedSection = false;
        bool createMenuOpen = false;
        bool hasCurrentSection = false;
        EditorMenuSectionId currentSection = EditorMenuSectionId::Edit;

        for (const IEditorAction* action : actions)
        {
            if (!action)
            {
                continue;
            }

            const EditorMenuSectionId actionSection = action->GetSection();
            if (!hasCurrentSection || actionSection != currentSection)
            {
                if (createMenuOpen)
                {
                    ImGui::EndMenu();
                    createMenuOpen = false;
                }

                if (hasRenderedSection)
                {
                    ImGui::Separator();
                }

                currentSection = actionSection;
                hasCurrentSection = true;
                hasRenderedSection = true;

                if (currentSection == EditorMenuSectionId::Create)
                {
                    createMenuOpen = ImGui::BeginMenu(GetEditorMenuSectionDisplayName(currentSection));
                }
            }

            if (currentSection == EditorMenuSectionId::Create && !createMenuOpen)
            {
                continue;
            }

            const bool enabled = action->CanExecute(ctx);
            DrawAction(editor, *action, ctx, enabled);
        }

        if (createMenuOpen)
        {
            ImGui::EndMenu();
        }
    }

    bool EditorMenuBuilder::EnsureSectionOpen(EditorMenuSectionId section)
    {
        if (section != EditorMenuSectionId::Create)
        {
            return false;
        }

        if (m_IsProviderCreateMenuOpen)
        {
            return true;
        }

        if (!ImGui::BeginMenu(GetEditorMenuSectionDisplayName(section)))
        {
            return false;
        }

        m_IsProviderCreateMenuOpen = true;
        return true;
    }

    void EditorMenuBuilder::DrawProviderMenuItem(
        IEditorContext& editor,
        const EditorMenuContext& ctx,
        const char* label,
        bool enabled,
        const char* disabledReason,
        const std::function<void(IEditorContext& editor, const EditorMenuContext& menuCtx)>& onExecute)
    {
        if (!label || !onExecute)
        {
            return;
        }

        if (!enabled)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::MenuItem(label))
        {
            onExecute(editor, ctx);
        }

        if (!enabled)
        {
            if (disabledReason && disabledReason[0] != '\0')
            {
                ImGui::SetItemTooltip("%s", disabledReason);
            }
            ImGui::EndDisabled();
        }
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
