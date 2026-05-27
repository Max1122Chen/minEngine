#include "ContextMenu/EditorActionRegistry.h"

#include "ContextMenu/EditorActionIds.h"
#include "ContextMenu/EditorMenuBuilder.h"
#include "ContextMenu/IEditorAction.h"

#include <algorithm>

namespace minEngine
{
    void EditorActionRegistry::Register(std::unique_ptr<IEditorAction> action)
    {
        if (!action)
        {
            return;
        }

        const EditorActionId actionId = action->GetId();
        for (const std::unique_ptr<IEditorAction>& existing : m_Actions)
        {
            if (existing && existing->GetId() == actionId)
            {
                return;
            }
        }

        m_Actions.push_back(std::move(action));
    }

    void EditorActionRegistry::RegisterProvider(EditorActionProvider provider)
    {
        if (!provider)
        {
            return;
        }
        m_Providers.push_back(std::move(provider));
    }

    std::vector<const IEditorAction*> EditorActionRegistry::Query(const EditorMenuContext& ctx) const
    {
        std::vector<const IEditorAction*> result;
        result.reserve(m_Actions.size());

        for (const std::unique_ptr<IEditorAction>& action : m_Actions)
        {
            if (action && action->IsVisibleInMenu(ctx))
            {
                result.push_back(action.get());
            }
        }

        std::sort(
            result.begin(),
            result.end(),
            [](const IEditorAction* left, const IEditorAction* right)
            {
                const int leftSectionOrder = GetEditorMenuSectionSortOrder(left->GetSection());
                const int rightSectionOrder = GetEditorMenuSectionSortOrder(right->GetSection());
                if (leftSectionOrder != rightSectionOrder)
                {
                    return leftSectionOrder < rightSectionOrder;
                }
                return left->GetSortOrder() < right->GetSortOrder();
            });
        return result;
    }

    void EditorActionRegistry::InvokeProviders(
        IEditorContext& editor,
        const EditorMenuContext& ctx,
        EditorMenuBuilder& builder) const
    {
        for (const EditorActionProvider& provider : m_Providers)
        {
            if (provider)
            {
                provider(editor, ctx, builder);
            }
        }
    }

    const IEditorAction* EditorActionRegistry::FindById(EditorActionId id) const
    {
        for (const std::unique_ptr<IEditorAction>& action : m_Actions)
        {
            if (action && action->GetId() == id)
            {
                return action.get();
            }
        }
        return nullptr;
    }

    void EditorActionRegistry::Clear()
    {
        m_Actions.clear();
        m_Providers.clear();
    }

    void EditorActionRegistry::ClearProviders()
    {
        m_Providers.clear();
    }

} // namespace minEngine
