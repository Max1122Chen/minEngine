#include "UI/EditorWindows/EditorWindow.h"

#include "Shell/EditorSubModule.h"

namespace minEngine
{
    bool EditorWindow::IsVisibleForActiveModule() const
    {
        // Shared panels (empty owner) stay eligible regardless of Active Session / type suite.
        const std::string_view ownerId = GetOwnerModuleId();
        if (ownerId.empty())
        {
            return true;
        }

        // Type-suite panels: visible when the foreground document-type controller matches.
        const EditorSubModule* active = m_Context.GetActiveSubModule();
        return active && active->GetModuleId() == ownerId;
    }
}
