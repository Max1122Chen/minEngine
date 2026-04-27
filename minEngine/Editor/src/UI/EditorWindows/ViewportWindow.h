#pragma once
#include "Core.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Function/Render/RenderCamera.h"

#include "UI/Widgets/DraggableOverlay.h"
#include "Viewport/EditorViewportClient.h"

#include "Editor.h"
#include "EditorWindow.h"

#include <algorithm>
#include <utility>

namespace minEngine
{
    class ViewportWindow final : public EditorWindow
    {
    public:
        explicit ViewportWindow(Editor& editor,
                                std::string id = "viewport",
                                std::string title = "Viewport")
            : EditorWindow(editor)
            , m_Id(std::move(id))
            , m_Title(std::move(title))
        {
        }

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }

        EditorViewportClient& GetViewportClient()
        {
            return m_Editor.GetOrCreateViewportClient(m_Id, m_Title);
        }

        void OnAttach() override
        {
            m_Editor.GetOrCreateViewportClient(m_Id, m_Title);
        }

        void OnDetach() override
        {
            m_Editor.RemoveViewportClient(m_Id);
        }

        virtual void OnDraw() override;

    private:
        void DrawGizmo(ViewportFrameState& frameState);

    private:
        std::string m_Id;
        std::string m_Title;
        UI::DraggableOverlayState m_OverlayState;
        UI::DraggableOverlayConfig m_OverlayConfig;
    };
}
