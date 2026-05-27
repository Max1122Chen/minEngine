#pragma once

#include <functional>

namespace minEngine
{
    class EditorMenuBuilder;
    class EditorMenuContext;
    class IEditorContext;

    using EditorActionProvider = std::function<void(
        IEditorContext& editor,
        const EditorMenuContext& ctx,
        EditorMenuBuilder& builder)>;

} // namespace minEngine
