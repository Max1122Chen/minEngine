#include "UI/Widgets/InlineRenameField.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <cctype>

namespace minEngine
{
    bool InlineRenameField::IsBlankName(const char* buffer)
    {
        if (buffer == nullptr)
        {
            return true;
        }

        for (const char* cursor = buffer; *cursor != '\0'; ++cursor)
        {
            const unsigned char character = static_cast<unsigned char>(*cursor);
            if (!std::isspace(character))
            {
                return false;
            }
        }

        return true;
    }

    InlineRenameField::Result InlineRenameField::Draw(char* buffer, size_t bufferSize, bool& requestFocus)
    {
        if (buffer == nullptr || bufferSize == 0)
        {
            return Result::Cancel;
        }

        // Sample Escape before InputText so a cancel is not lost if ImGui consumes the key.
        const bool escapePressed = ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (requestFocus)
        {
            ImGui::SetKeyboardFocusHere();
            requestFocus = false;
        }

        const bool enterPressed = ImGui::InputText(
            "##InlineRename",
            buffer,
            bufferSize,
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);

        if (escapePressed)
        {
            if (ImGui::IsItemActive())
            {
                ImGui::ClearActiveID();
            }
            return Result::Cancel;
        }

        if (enterPressed)
        {
            if (IsBlankName(buffer))
            {
                return Result::Cancel;
            }
            return Result::Commit;
        }

        // Click-away / focus loss without Enter: always cancel (even if text was not edited).
        // IsItemDeactivatedAfterEdit only fires after edits — that left the field stuck open.
        if (ImGui::IsItemDeactivated())
        {
            return Result::Cancel;
        }

        return Result::Editing;
    }
}
