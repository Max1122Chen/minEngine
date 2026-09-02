#pragma once

#include "Runtime/Core/Command/CommandTypes.h"
#include "UI/Appearance/EditorAppearance.h"

#include "imgui.h"

namespace minEngine
{
    class CommandConsoleStyle
    {
    public:
        explicit CommandConsoleStyle(const EditorAppearance& appearance);

        ImVec4 GetColor(Command::CommandOutputKind kind) const;

    private:
        const EditorAppearance& m_Appearance;
    };
}
