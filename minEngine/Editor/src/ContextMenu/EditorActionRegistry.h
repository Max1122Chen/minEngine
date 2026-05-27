#pragma once

#include "Core.h"

#include "ContextMenu/IEditorAction.h"

#include <functional>
#include <memory>
#include <vector>

namespace minEngine
{
    class EditorMenuBuilder;
    class EditorMenuContext;
    class IEditorContext;

    using EditorActionProvider = std::function<void(
        IEditorContext& editor,
        const EditorMenuContext& ctx,
        EditorMenuBuilder& builder)>;

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
