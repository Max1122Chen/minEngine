#pragma once

namespace minEngine
{
    class Editor;
    struct EditorState;

    struct PanelContext
    {
        Editor* editor = nullptr;
        EditorState* state = nullptr;
        float deltaTime = 0.0f;
    };
}
