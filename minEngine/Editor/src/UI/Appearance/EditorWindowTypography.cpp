#include "UI/Appearance/EditorWindowTypography.h"

#include "Shell/IEditorContext.h"
#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorTypographyScope.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

namespace minEngine
{
    bool EditorWindowTypography::BeginPanel(IEditorContext& context,
                                            const char* title,
                                            bool* open,
                                            ImGuiWindowFlags flags)
    {
        bool windowOpen = false;
        {
            EditorTypographyScope headingTypography(
                context.GetEditorAppearance(),
                EditorTypographyRole::Heading);
            if (open != nullptr)
            {
                windowOpen = ImGui::Begin(title, open, flags);
            }
            else
            {
                windowOpen = ImGui::Begin(title, nullptr, flags);
            }
        }

        if (!windowOpen)
        {
            ImGui::End();
        }

        return windowOpen;
    }
}
