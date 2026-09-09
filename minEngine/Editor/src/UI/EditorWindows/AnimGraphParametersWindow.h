#pragma once

#include "Core.h"
#include "EditorWindow.h"

namespace minEngine
{
    class AnimGraphParametersWindow final : public EditorWindow
    {
    public:
        explicit AnimGraphParametersWindow(IEditorContext& context);

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }
        std::string_view GetOwnerModuleId() const override;

        void OnDraw() override;

    private:
        void DrawSchemaTable();

        const std::string m_Id = "anim_graph_parameters";
        const std::string m_Title = "Anim Graph Parameters";
    };
}
