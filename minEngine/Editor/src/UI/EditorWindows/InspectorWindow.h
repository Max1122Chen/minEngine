#pragma once

#include "Core.h"
#include "UI/EditorWindows/EditorWindow.h"

namespace minEngine
{
    class InspectorWindow final : public EditorWindow
    {
    public:
        explicit InspectorWindow(IEditorContext& context)
            : EditorWindow(context)
        {
        }

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }

        void OnDraw() override;

    private:
        const std::string m_Id = "inspector";
        const std::string m_Title = "Inspector";
    };
}
