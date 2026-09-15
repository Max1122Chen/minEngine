#pragma once

#include "SmGraphTypes.h"

#include "imgui_canvas.h"

namespace minEngine::SmGraph
{
    struct Style
    {
        float NodeRounding = 6.0f;
        float EdgeHitThreshold = 6.0f;
        float EdgeRingThickness = 24.0f;      // exterior invisible link hit pad
        float BodyInset = 6.0f;              // visual ring thickness
        float HitBodyInset = 12.0f;          // hit ring (wider than visual)
        float NodeContentPadX = 10.0f;
        float NodeContentPadY = 8.0f;
        float NodeTitleGap = 4.0f;
        float MinScale = 0.25f;
        float MaxScale = 2.5f;
        float ZoomAnimDuration = 0.15f;   // ax c_MouseZoomDuration
        ImVec2 MinNodeSize{120.0f, 48.0f};
        ImVec2 MinSpecialNodeSize{100.0f, 40.0f};

        // Defaults; host should call ApplyEditorTheme each frame for dark/light.
        ImU32 GridColor = IM_COL32(60, 60, 60, 70);
        ImU32 NodeRingFill = IM_COL32(60, 60, 60, 255);
        ImU32 NodeRingFillSelected = IM_COL32(55, 75, 105, 255);
        ImU32 NodeBodyFill = IM_COL32(45, 45, 47, 255);
        ImU32 NodeBodyBorder = IM_COL32(70, 70, 70, 255);
        ImU32 NodeBorder = IM_COL32(80, 80, 80, 255);
        ImU32 NodeBorderSelected = IM_COL32(70, 150, 255, 255);   // accent blue
        ImU32 NodeTitle = IM_COL32(220, 220, 220, 255);
        ImU32 NodeSubtitle = IM_COL32(160, 160, 160, 255);
        ImU32 EntryRingFill = IM_COL32(45, 70, 55, 255);
        ImU32 AnyStateRingFill = IM_COL32(70, 55, 45, 255);
        ImU32 EdgeColor = IM_COL32(180, 190, 210, 230);           // brighter than chrome gray
        ImU32 EdgeSelected = IM_COL32(70, 150, 255, 255);         // accent blue
        ImU32 EntryEdgeColor = IM_COL32(120, 200, 140, 220);
        ImU32 AnyStateEdgeColor = IM_COL32(220, 160, 110, 220);
        ImU32 LinkPreview = IM_COL32(240, 180, 60, 230);          // warm yellow
        ImU32 HoverTarget = IM_COL32(240, 180, 60, 255);
    };

    /** ImGui-only state-machine graph widget. No engine domain types. */
    class Widget
    {
    public:
        void ResetInteraction();
        void Draw(const char* id, Document& document, std::vector<EditEvent>& outEvents);

        const Style& GetStyle() const { return m_Style; }
        Style& GetStyle() { return m_Style; }

    private:
        enum class Mode : uint8_t
        {
            Idle = 0,
            DragNode = 1,
            LinkDrag = 2,
            Pan = 3,
            BoxSelect = 4,
        };

        enum class HitKind : uint8_t
        {
            None = 0,
            NodeBody = 1,
            NodeRing = 2,
            Edge = 3,
        };

        enum class ContextMenuTarget : uint8_t
        {
            None = 0,
            Background = 1,
            Edge = 2,
            Node = 3,
        };

        struct HitResult
        {
            HitKind Kind = HitKind::None;
            NodeId Node = kInvalidNodeId;
            EdgeId Edge = kInvalidEdgeId;
        };

        void LayoutNodeSizes(Document& document) const;
        void DrawGrid(ImDrawList* drawList, const ImRect& viewRect) const;
        void DrawNodes(ImDrawList* drawList, Document& document, NodeId hoverTarget) const;
        void DrawEdges(ImDrawList* drawList, const Document& document) const;
        void DrawLinkPreview(ImDrawList* drawList, const Document& document) const;
        void DrawBoxSelectOverlay(ImDrawList* drawList) const;

        HitResult HitTest(const Document& document, const ImVec2& canvasPos) const;
        bool HitEdge(const Document& document, const Edge& edge, const ImVec2& canvasPos) const;
        bool CanCreateEdge(const Document& document, NodeId from, NodeId to) const;

        void SetSelection(Document& document, Selection selection, std::vector<EditEvent>& outEvents);
        void EmitSelectionChanged(const Document& document, std::vector<EditEvent>& outEvents) const;
        void HandleIdleInput(Document& document, std::vector<EditEvent>& outEvents);
        void HandleDragNode(Document& document, std::vector<EditEvent>& outEvents);
        void HandleLinkDrag(Document& document, std::vector<EditEvent>& outEvents);
        void HandlePan();
        void HandleBoxSelect(Document& document, std::vector<EditEvent>& outEvents);
        void QueueContextMenuFromHit(const HitResult& hit, const ImVec2& canvasPos, Document& document,
                                     std::vector<EditEvent>& outEvents);
        NodeId PickBoxSelectPrimary(const Document& document, const ImRect& selectRect) const;
        void HandleZoom();
        void UpdateNavigationAnimation();
        void CancelNavigationAnimation();
        void ApplyScrollZoomToView();
        void SetViewRect(const ImRect& rect);
        ImRect GetViewRect() const;
        float GetNextZoom(float wheelSteps) const;
        void HandleShortcuts(Document& document, std::vector<EditEvent>& outEvents);
        void HandleContextMenu(Document& document, std::vector<EditEvent>& outEvents);

        static ImVec2 NodeCenter(const Node& node);
        static ImVec2 ClosestPointOnRectBorder(const ImVec2& rectMin, const ImVec2& rectMax, const ImVec2& toward);
        static void DrawArrowHead(ImDrawList* drawList, const ImVec2& tip, const ImVec2& direction, ImU32 color);
        static float DistancePointToSegment(const ImVec2& point, const ImVec2& a, const ImVec2& b);
        static ImRect MakeNormalizedRect(const ImVec2& a, const ImVec2& b);

        Style m_Style;
        ImGuiEx::Canvas m_Canvas;
        ImGuiEx::CanvasView m_View{ImVec2(0.0f, 0.0f), 1.0f};

        // Matches ax::NavigateAction: Origin = -Scroll, Scale = Zoom.
        ImVec2 m_Scroll{0.0f, 0.0f};
        float m_Zoom = 1.0f;
        ImVec2 m_ScrollStart{0.0f, 0.0f};

        Mode m_Mode = Mode::Idle;
        ImGuiMouseButton m_PanButton = ImGuiMouseButton_Middle;
        NodeId m_ActiveNode = kInvalidNodeId;
        ImVec2 m_DragGrabOffset{0.0f, 0.0f};
        ImVec2 m_ContextMenuCanvasPos{0.0f, 0.0f};
        NodeId m_LinkHoverTarget = kInvalidNodeId;
        ContextMenuTarget m_PendingContextMenu = ContextMenuTarget::None;
        EdgeId m_ContextMenuEdge = kInvalidEdgeId;
        NodeId m_ContextMenuNode = kInvalidNodeId;
        bool m_OpenRenamePopup = false;
        char m_RenameBuffer[128]{};

        bool m_RightGestureActive = false;
        HitResult m_RightGestureHit{};
        ImVec2 m_BoxSelectStart{0.0f, 0.0f};
        ImVec2 m_BoxSelectEnd{0.0f, 0.0f};

        // View-rect animation (same approach as ed::NavigateAnimation).
        bool m_NavAnimating = false;
        float m_NavTime = 0.0f;
        float m_NavDuration = 0.0f;
        ImRect m_NavStart{};
        ImRect m_NavTarget{};
    };
}
