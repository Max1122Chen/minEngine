#include "UI/EditorWindows/EditorWindow.h"

#include "Shell/EditorSubModule.h"

namespace minEngine
{
    bool EditorWindow::IsVisibleForActiveModule() const
    {
        const std::string_view ownerId = GetOwnerModuleId();
        if (ownerId.empty())
        {
            return true;
        }

        const EditorSubModule* active = m_Context.GetActiveSubModule();
        return active && active->GetModuleId() == ownerId;
    }
}
