#pragma once

#include <functional>
#include <string_view>

struct ImVec2;

namespace minEngine
{
    struct Transform;
    class EditorAppearance;

    class TransformWidget
    {
    public:
        static bool Draw(Transform* transform,
                         int treeFlags,
                         const std::function<void(std::string_view fieldName)>& applyUndoForField,
                         EditorAppearance* appearance = nullptr);
    };
}

