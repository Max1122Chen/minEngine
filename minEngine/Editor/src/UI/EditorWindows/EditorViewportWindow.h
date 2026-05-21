#pragma once

#include "Core.h"

#include "imgui.h"

#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Viewport/EditorViewportClient.h"
#include "EditorWindow.h"

namespace minEngine
{
    class Editor;

    /** Base dock panel that displays a scene render target and forwards frame rects to a viewport client. */
    class EditorViewportWindow : public EditorWindow
    {
    public:
        explicit EditorViewportWindow(Editor& editor, std::string id, std::string title)
            : EditorWindow(editor)
            , m_Id(std::move(id))
            , m_Title(std::move(title))
        {
        }

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }

        void OnAttach() override;
        void OnDetach() override;

        void OnDraw() override final;

        EditorViewportClient& GetViewportClient();
        const EditorViewportClient& GetViewportClient() const;

    protected:
        virtual EditorViewportClient& GetOrCreateViewportClient() = 0;
        virtual const std::shared_ptr<RHITexture2D>& GetDisplayColorTexture() const = 0;
        virtual ImGuiWindowFlags GetViewportWindowFlags() const;
        virtual void OnDrawViewportOverlay(EditorViewportClient& client, const ViewportFrameState& frameState);

        const std::string& GetViewportPanelId() const { return m_Id; }

        std::string m_Id;
        std::string m_Title;

    private:
        bool DrawSceneColorImage(ViewportFrameState& outFrameState);
    };
}
