#pragma once

#include "Core.h"
#include "EditorWindow.h"

namespace minEngine
{
    class MaterialDetailsWindow final : public EditorWindow
    {
    public:
        explicit MaterialDetailsWindow(Editor& editor)
            : EditorWindow(editor)
        {
            SetOpen(false);
        }

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }
        EditorWindowSuite GetWindowSuite() const override { return EditorWindowSuite::MaterialEditing; }

        void OnDraw() override;

    private:
        const std::string m_Id = "material_details";
        const std::string m_Title = "Material Details";
    };
}
