#pragma once

#include "Core.h"

#include "ContextMenu/EditorActionRegistry.h"

#include <memory>

namespace minEngine
{
    class EditorMenuBuilder;
    class EditorMenuContext;
    class IEditorContext;

    class EditorContextMenuSystem
    {
    public:
        EditorContextMenuSystem();
        ~EditorContextMenuSystem();

        void RegisterBuiltInActions();
        void Shutdown();

        void BuildAndDraw(IEditorContext& editor, const EditorMenuContext& ctx);

        EditorActionRegistry& GetRegistry();
        const EditorActionRegistry& GetRegistry() const;

    private:
        EditorActionRegistry m_Registry;
        std::unique_ptr<EditorMenuBuilder> m_Builder;
    };

} // namespace minEngine
