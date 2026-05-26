#pragma once

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

namespace minEngine
{
    class EditorAppearance;

    class EditorTypographyScope
    {
    public:
        EditorTypographyScope(EditorAppearance& appearance, EditorTypographyRole role);
        ~EditorTypographyScope();

        EditorTypographyScope(const EditorTypographyScope&) = delete;
        EditorTypographyScope& operator=(const EditorTypographyScope&) = delete;

    private:
        bool m_Pushed = false;
    };
}
