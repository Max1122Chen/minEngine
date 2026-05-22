#pragma once

namespace minEngine
{
    enum class EditorUIMode
    {
        SceneEditing,
        MaterialEditing,
    };

    enum class EditorWindowSuite
    {
        Shared,
        SceneEditing,
        MaterialEditing,
    };

    inline bool IsWindowActiveForUIMode(EditorWindowSuite suite, EditorUIMode mode)
    {
        if (suite == EditorWindowSuite::Shared)
        {
            return true;
        }

        if (mode == EditorUIMode::SceneEditing)
        {
            return suite == EditorWindowSuite::SceneEditing;
        }

        return suite == EditorWindowSuite::MaterialEditing;
    }
}
