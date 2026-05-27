#pragma once

#include "Core.h"

#include "ContextMenu/EditorActionProviderTypes.h"
#include "ContextMenu/IEditorAction.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class EditorMenuBuilder;
    class EditorMenuContext;
    class IEditorContext;

    class EditorActionRegistry
    {
    public:
        void Register(std::unique_ptr<IEditorAction> action);
        void RegisterProvider(EditorActionProvider provider);

        std::vector<const IEditorAction*> Query(const EditorMenuContext& ctx) const;
        void InvokeProviders(IEditorContext& editor, const EditorMenuContext& ctx, EditorMenuBuilder& builder) const;

        const IEditorAction* FindById(EditorActionId id) const;

        void Clear();
        void ClearProviders();

    private:
        std::vector<std::unique_ptr<IEditorAction>> m_Actions;
        std::vector<EditorActionProvider> m_Providers;
    };

} // namespace minEngine
