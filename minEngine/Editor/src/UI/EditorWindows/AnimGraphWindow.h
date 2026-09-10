#pragma once

#include "Core.h"
#include "EditorWindow.h"

#include "SubEditor/AnimationGraph/AnimGraphSmBridge.h"
#include "UI/SmGraph/SmGraphWidget.h"

namespace minEngine
{
    class AnimGraphWindow final : public EditorWindow
    {
    public:
        explicit AnimGraphWindow(IEditorContext& context);
        ~AnimGraphWindow() override = default;

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }
        std::string_view GetOwnerModuleId() const override;

        void OnDraw() override;

    private:
        void DrawToolbar();
        void DrawCanvas();

        const std::string m_Id = "anim_graph";
        const std::string m_Title = "Anim Graph";

        SmGraph::Widget m_SmGraphWidget;
        SmGraph::Document m_SmGraphDocument;
        AnimGraphSpecialNodeLayout m_SpecialNodeLayout;
        AnimationGraph* m_BoundGraph = nullptr;
    };
}
