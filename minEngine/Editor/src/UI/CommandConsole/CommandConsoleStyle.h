#pragma once

#include "Runtime/Core/Command/CommandTypes.h"
#include "UI/Appearance/EditorAppearance.h"

#include "imgui.h"

namespace minEngine
{
    struct CommandCompletionRowStyle
    {
        ImVec4 LabelColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        ImVec4 DescriptionColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        ImU32 SelectionBackground = 0;
        ImU32 SelectionBar = 0;
    };

    class CommandConsoleStyle
    {
    public:
        explicit CommandConsoleStyle(const EditorAppearance& appearance);

        ImVec4 GetColor(Command::CommandOutputKind kind) const;
        CommandCompletionRowStyle GetCompletionRowStyle(bool selected) const;

    private:
        const EditorAppearance& m_Appearance;
    };
}
