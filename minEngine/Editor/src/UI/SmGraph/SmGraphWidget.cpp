#include "SmGraphWidget.h"

#include <algorithm>
#include <cmath>

namespace minEngine::SmGraph
{
    void Widget::ResetInteraction()
    {
        m_Mode = Mode::Idle;
        m_ActiveNode = kInvalidNodeId;
        m_DragGrabOffset = ImVec2(0.0f, 0.0f);
        m_LinkHoverTarget = kInvalidNodeId;
        m_OpenContextMenu = false;
        CancelNavigationAnimation();
    }

    void Widget::Draw(const char* id, Document& document, std::vector<EditEvent>& outEvents)
    {
        outEvents.clear();

        if (!m_Canvas.Begin(id, ImVec2(0.0f, 0.0f)))
        {
            return;
        }

        // Advance zoom animation before drawing so the frame matches the eased view.
        UpdateNavigationAnimation();
        m_Canvas.SetView(m_View);

        LayoutNodeSizes(document);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        DrawGrid(drawList, m_Canvas.ViewRect());
        DrawEdges(drawList, document);
        DrawNodes(drawList, document, m_LinkHoverTarget);
        if (m_Mode == Mode::LinkDrag)
        {
            DrawLinkPreview(drawList, document);
        }

        HandleZoom();

        switch (m_Mode)
        {
        case Mode::DragNode:
            HandleDragNode(document, outEvents);
            break;
        case Mode::LinkDrag:
            HandleLinkDrag(document, outEvents);
            break;
        case Mode::Pan:
            HandlePan();
            break;
        case Mode::Idle:
        default:
            HandleIdleInput(document, outEvents);
            break;
        }

        HandleShortcuts(document, outEvents);
        HandleContextMenu(document, outEvents);

        ApplyScrollZoomToView();
        m_Canvas.SetView(m_View);
        m_Canvas.End();
    }


    void Widget::LayoutNodeSizes(Document& document) const
    {
        const float inset = m_Style.BodyInset;
        const float padX = m_Style.NodeContentPadX;
        const float padY = m_Style.NodeContentPadY;
        const float gap = m_Style.NodeTitleGap;

        for (Node& node : document.GetNodes())
        {
            const char* title = node.Title.empty() ? "(unnamed)" : node.Title.c_str();
            const char* subtitle = node.Subtitle.empty() ? "(no clip)" : node.Subtitle.c_str();
            const ImVec2 titleSize = ImGui::CalcTextSize(title);
            const ImVec2 subtitleSize = ImGui::CalcTextSize(subtitle);

            const float contentW = (std::max)(titleSize.x, subtitleSize.x) + padX * 2.0f;
            const float contentH = titleSize.y + gap + subtitleSize.y + padY * 2.0f;
            const float width = (std::max)(contentW + inset * 2.0f, m_Style.MinNodeSize.x);
            const float height = (std::max)(contentH + inset * 2.0f, m_Style.MinNodeSize.y);
            node.Size = ImVec2(width, height);
        }
    }

    void Widget::DrawGrid(ImDrawList* drawList, const ImRect& viewRect) const
    {
        constexpr float kGridStep = 64.0f;
        const float startX = std::floor(viewRect.Min.x / kGridStep) * kGridStep;
        const float startY = std::floor(viewRect.Min.y / kGridStep) * kGridStep;

        for (float x = startX; x < viewRect.Max.x; x += kGridStep)
        {
            drawList->AddLine(ImVec2(x, viewRect.Min.y), ImVec2(x, viewRect.Max.y), m_Style.GridColor);
        }
        for (float y = startY; y < viewRect.Max.y; y += kGridStep)
        {
            drawList->AddLine(ImVec2(viewRect.Min.x, y), ImVec2(viewRect.Max.x, y), m_Style.GridColor);
        }
    }

    void Widget::DrawNodes(ImDrawList* drawList, Document& document, NodeId hoverTarget) const
    {
        const Selection& selection = document.GetSelection();
        const float inset = m_Style.BodyInset;
        const float padX = m_Style.NodeContentPadX;
        const float padY = m_Style.NodeContentPadY;
        const float gap = m_Style.NodeTitleGap;
        const float innerRounding = (std::max)(2.0f, m_Style.NodeRounding - 2.0f);

        for (const Node& node : document.GetNodes())
        {
            const ImVec2 max = ImVec2(node.Pos.x + node.Size.x, node.Pos.y + node.Size.y);
            const ImVec2 bodyMin(node.Pos.x + inset, node.Pos.y + inset);
            const ImVec2 bodyMax(max.x - inset, max.y - inset);
            const bool selected =
                selection.Kind == SelectionKind::Node && selection.Node == node.Id;
            const bool hoveredTarget = hoverTarget == node.Id;

            ImU32 border = selected ? m_Style.NodeBorderSelected : m_Style.NodeBorder;
            if (hoveredTarget)
            {
                border = m_Style.HoverTarget;
            }

            const ImU32 ringFill = selected ? m_Style.NodeRingFillSelected : m_Style.NodeRingFill;

            drawList->AddRectFilled(node.Pos, max, ringFill, m_Style.NodeRounding);
            drawList->AddRectFilled(bodyMin, bodyMax, m_Style.NodeBodyFill, innerRounding);
            drawList->AddRect(bodyMin, bodyMax, m_Style.NodeBodyBorder, innerRounding, 0, 1.0f);

            if (selected)
            {
                // Soft outer glow + strong accent stroke.
                drawList->AddRect(
                    ImVec2(node.Pos.x - 2.0f, node.Pos.y - 2.0f),
                    ImVec2(max.x + 2.0f, max.y + 2.0f),
                    m_Style.NodeBorderSelected,
                    m_Style.NodeRounding + 1.0f,
                    0,
                    1.5f);
            }

            drawList->AddRect(
                node.Pos,
                max,
                border,
                m_Style.NodeRounding,
                0,
                selected || hoveredTarget ? 2.5f : 1.5f);

            const char* title = node.Title.empty() ? "(unnamed)" : node.Title.c_str();
            const char* subtitle = node.Subtitle.empty() ? "(no clip)" : node.Subtitle.c_str();
            const ImVec2 titlePos(bodyMin.x + padX, bodyMin.y + padY);
            const ImVec2 titleSize = ImGui::CalcTextSize(title);
            const ImVec2 subtitlePos(bodyMin.x + padX, titlePos.y + titleSize.y + gap);
            drawList->AddText(titlePos, m_Style.NodeTitle, title);
            drawList->AddText(subtitlePos, m_Style.NodeSubtitle, subtitle);
        }
    }

    void Widget::DrawEdges(ImDrawList* drawList, const Document& document) const
    {
        const Selection& selection = document.GetSelection();

        for (const Edge& edge : document.GetEdges())
        {
            const Node* from = document.FindNode(edge.From);
            const Node* to = document.FindNode(edge.To);
            if (!from || !to)
            {
                continue;
            }

            const ImVec2 fromCenter = NodeCenter(*from);
            const ImVec2 toCenter = NodeCenter(*to);
            const ImVec2 fromBorder = ClosestPointOnRectBorder(
                from->Pos,
                ImVec2(from->Pos.x + from->Size.x, from->Pos.y + from->Size.y),
                toCenter);
            const ImVec2 toBorder = ClosestPointOnRectBorder(
                to->Pos,
                ImVec2(to->Pos.x + to->Size.x, to->Pos.y + to->Size.y),
                fromCenter);

            const bool selected =
                selection.Kind == SelectionKind::Edge && selection.Edge == edge.Id;
            const ImU32 color = selected ? m_Style.EdgeSelected : m_Style.EdgeColor;
            const float thickness = selected ? 3.0f : 2.0f;

            drawList->AddLine(fromBorder, toBorder, color, thickness);
            DrawArrowHead(
                drawList,
                toBorder,
                ImVec2(toBorder.x - fromBorder.x, toBorder.y - fromBorder.y),
                color);
        }
    }

    void Widget::DrawLinkPreview(ImDrawList* drawList, const Document& document) const
    {
        const Node* from = document.FindNode(m_ActiveNode);
        if (!from)
        {
            return;
        }

        const ImVec2 fromCenter = NodeCenter(*from);
        const ImVec2 mouse = ImGui::GetMousePos();
        const ImVec2 fromBorder = ClosestPointOnRectBorder(
            from->Pos,
            ImVec2(from->Pos.x + from->Size.x, from->Pos.y + from->Size.y),
            mouse);

        drawList->AddLine(fromBorder, mouse, m_Style.LinkPreview, 2.0f);
        DrawArrowHead(
            drawList,
            mouse,
            ImVec2(mouse.x - fromCenter.x, mouse.y - fromCenter.y),
            m_Style.LinkPreview);
    }

    Widget::HitResult Widget::HitTest(const Document& document, const ImVec2& canvasPos) const
    {
        HitResult result;

        for (int i = static_cast<int>(document.GetNodes().size()) - 1; i >= 0; --i)
        {
            const Node& node = document.GetNodes()[static_cast<size_t>(i)];
            const ImVec2 max(node.Pos.x + node.Size.x, node.Pos.y + node.Size.y);
            const float inset = m_Style.HitBodyInset;
            const ImRect outer(
                ImVec2(node.Pos.x - m_Style.EdgeRingThickness, node.Pos.y - m_Style.EdgeRingThickness),
                ImVec2(max.x + m_Style.EdgeRingThickness, max.y + m_Style.EdgeRingThickness));
            // Hit body inset can be wider than the visual ring so LinkDrag is easier.
            const ImRect body(
                ImVec2(node.Pos.x + inset, node.Pos.y + inset),
                ImVec2(max.x - inset, max.y - inset));

            if (outer.Contains(canvasPos) && !body.Contains(canvasPos))
            {
                result.Kind = HitKind::NodeRing;
                result.Node = node.Id;
                return result;
            }
        }

        for (int i = static_cast<int>(document.GetEdges().size()) - 1; i >= 0; --i)
        {
            const Edge& edge = document.GetEdges()[static_cast<size_t>(i)];
            if (HitEdge(document, edge, canvasPos))
            {
                result.Kind = HitKind::Edge;
                result.Edge = edge.Id;
                return result;
            }
        }

        for (int i = static_cast<int>(document.GetNodes().size()) - 1; i >= 0; --i)
        {
            const Node& node = document.GetNodes()[static_cast<size_t>(i)];
            const float inset = m_Style.HitBodyInset;
            const ImRect body(
                ImVec2(node.Pos.x + inset, node.Pos.y + inset),
                ImVec2(node.Pos.x + node.Size.x - inset, node.Pos.y + node.Size.y - inset));
            if (body.Contains(canvasPos))
            {
                result.Kind = HitKind::NodeBody;
                result.Node = node.Id;
                return result;
            }
        }

        return result;
    }

    bool Widget::HitEdge(const Document& document, const Edge& edge, const ImVec2& canvasPos) const
    {
        const Node* from = document.FindNode(edge.From);
        const Node* to = document.FindNode(edge.To);
        if (!from || !to)
        {
            return false;
        }

        const ImVec2 fromCenter = NodeCenter(*from);
        const ImVec2 toCenter = NodeCenter(*to);
        const ImVec2 fromBorder = ClosestPointOnRectBorder(
            from->Pos,
            ImVec2(from->Pos.x + from->Size.x, from->Pos.y + from->Size.y),
            toCenter);
        const ImVec2 toBorder = ClosestPointOnRectBorder(
            to->Pos,
            ImVec2(to->Pos.x + to->Size.x, to->Pos.y + to->Size.y),
            fromCenter);

        return DistancePointToSegment(canvasPos, fromBorder, toBorder) <= m_Style.EdgeHitThreshold;
    }

    void Widget::SetSelection(Document& document, Selection selection, std::vector<EditEvent>& outEvents)
    {
        if (document.GetSelection().Equals(selection))
        {
            return;
        }

        document.GetSelection() = selection;
        EmitSelectionChanged(document, outEvents);
    }

    void Widget::EmitSelectionChanged(const Document& document, std::vector<EditEvent>& outEvents) const
    {
        EditEvent event;
        event.Kind = EditKind::SelectionChanged;
        event.Node = document.GetSelection().Node;
        event.Edge = document.GetSelection().Edge;
        outEvents.push_back(event);
    }

    void Widget::HandleIdleInput(Document& document, std::vector<EditEvent>& outEvents)
    {
        const ImGuiIO& io = ImGui::GetIO();
        const ImVec2 mouse = ImGui::GetMousePos();

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)
            || (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyAlt))
        {
            CancelNavigationAnimation();
            m_ScrollStart = m_Scroll;
            m_Mode = Mode::Pan;
            return;
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !io.KeyAlt)
        {
            const HitResult hit = HitTest(document, mouse);
            if (hit.Kind == HitKind::None)
            {
                m_ContextMenuCanvasPos = mouse;
                m_OpenContextMenu = true;
            }
            return;
        }

        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || io.KeyAlt)
        {
            return;
        }

        const HitResult hit = HitTest(document, mouse);
        switch (hit.Kind)
        {
        case HitKind::NodeRing:
        {
            Selection selection;
            selection.Kind = SelectionKind::Node;
            selection.Node = hit.Node;
            SetSelection(document, selection, outEvents);

            m_Mode = Mode::LinkDrag;
            m_ActiveNode = hit.Node;
            m_LinkHoverTarget = kInvalidNodeId;
            break;
        }
        case HitKind::Edge:
        {
            Selection selection;
            selection.Kind = SelectionKind::Edge;
            selection.Edge = hit.Edge;
            SetSelection(document, selection, outEvents);
            break;
        }
        case HitKind::NodeBody:
        {
            Selection selection;
            selection.Kind = SelectionKind::Node;
            selection.Node = hit.Node;
            SetSelection(document, selection, outEvents);

            if (Node* node = document.FindNode(hit.Node))
            {
                m_Mode = Mode::DragNode;
                m_ActiveNode = hit.Node;
                m_DragGrabOffset = ImVec2(mouse.x - node->Pos.x, mouse.y - node->Pos.y);
            }
            break;
        }
        case HitKind::None:
        default:
            SetSelection(document, Selection{}, outEvents);
            break;
        }
    }

    void Widget::HandleDragNode(Document& document, std::vector<EditEvent>& outEvents)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (Node* node = document.FindNode(m_ActiveNode))
            {
                EditEvent event;
                event.Kind = EditKind::NodeMoved;
                event.Node = node->Id;
                event.Pos = node->Pos;
                outEvents.push_back(event);
            }
            m_Mode = Mode::Idle;
            m_ActiveNode = kInvalidNodeId;
            return;
        }

        if (Node* node = document.FindNode(m_ActiveNode))
        {
            const ImVec2 mouse = ImGui::GetMousePos();
            node->Pos = ImVec2(mouse.x - m_DragGrabOffset.x, mouse.y - m_DragGrabOffset.y);

            EditEvent event;
            event.Kind = EditKind::NodeMoved;
            event.Node = node->Id;
            event.Pos = node->Pos;
            outEvents.push_back(event);
        }
    }

    void Widget::HandleLinkDrag(Document& document, std::vector<EditEvent>& outEvents)
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        m_LinkHoverTarget = kInvalidNodeId;

        const HitResult hit = HitTest(document, mouse);
        if ((hit.Kind == HitKind::NodeBody || hit.Kind == HitKind::NodeRing)
            && hit.Node != m_ActiveNode)
        {
            m_LinkHoverTarget = hit.Node;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            m_Mode = Mode::Idle;
            m_ActiveNode = kInvalidNodeId;
            m_LinkHoverTarget = kInvalidNodeId;
            return;
        }

        if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            return;
        }

        if (m_LinkHoverTarget != kInvalidNodeId && m_LinkHoverTarget != m_ActiveNode)
        {
            EditEvent event;
            event.Kind = EditKind::CreateEdgeRequested;
            event.From = m_ActiveNode;
            event.To = m_LinkHoverTarget;
            outEvents.push_back(event);
        }

        m_Mode = Mode::Idle;
        m_ActiveNode = kInvalidNodeId;
        m_LinkHoverTarget = kInvalidNodeId;
    }

    void Widget::CancelNavigationAnimation()
    {
        m_NavAnimating = false;
        m_NavTime = 0.0f;
        m_NavDuration = 0.0f;
    }

    void Widget::ApplyScrollZoomToView()
    {
        // ax NavigateAction::GetView() == CanvasView(-m_Scroll, m_Zoom)
        m_View.Set(ImVec2(-m_Scroll.x, -m_Scroll.y), m_Zoom);
    }

    void Widget::SetViewRect(const ImRect& rect)
    {
        const ImGuiEx::CanvasView view = m_Canvas.CalcCenterView(rect);
        m_Scroll = ImVec2(-view.Origin.x, -view.Origin.y);
        m_Zoom = view.Scale;
        ApplyScrollZoomToView();
    }

    ImRect Widget::GetViewRect() const
    {
        return m_Canvas.CalcViewRect(ImGuiEx::CanvasView(ImVec2(-m_Scroll.x, -m_Scroll.y), m_Zoom));
    }

    float Widget::GetNextZoom(float wheelSteps) const
    {
        // Same discrete ladder as imgui-node-editor NavigateAction (Windows default).
        static const float kZoomLevels[] = {
            0.1f, 0.15f, 0.20f, 0.25f, 0.33f, 0.5f, 0.75f, 1.0f, 1.25f, 1.50f,
            2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
        static const int kZoomLevelCount = static_cast<int>(sizeof(kZoomLevels) / sizeof(kZoomLevels[0]));

        const int direction = (wheelSteps > 0.0f) ? 1 : ((wheelSteps < 0.0f) ? -1 : 0);
        if (direction == 0)
        {
            return m_Zoom;
        }

        int bestIndex = 0;
        float bestDistance = std::abs(kZoomLevels[0] - m_Zoom);
        for (int i = 1; i < kZoomLevelCount; ++i)
        {
            const float distance = std::abs(kZoomLevels[i] - m_Zoom);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = i;
            }
        }

        if (bestDistance > 0.001f)
        {
            bestIndex += direction;
            bestIndex = std::clamp(bestIndex, 0, kZoomLevelCount - 1);
            return std::clamp(kZoomLevels[bestIndex], m_Style.MinScale, m_Style.MaxScale);
        }

        const int newIndex = std::clamp(bestIndex + direction, 0, kZoomLevelCount - 1);
        return std::clamp(kZoomLevels[newIndex], m_Style.MinScale, m_Style.MaxScale);
    }

    void Widget::UpdateNavigationAnimation()
    {
        if (!m_NavAnimating)
        {
            ApplyScrollZoomToView();
            return;
        }

        m_NavTime += (std::max)(0.0f, ImGui::GetIO().DeltaTime);
        if (m_NavTime < m_NavDuration)
        {
            const float progress = (m_NavDuration > 0.0f) ? (m_NavTime / m_NavDuration) : 1.0f;
            // Same formula as ImEasing::EaseOutQuad used by ax NavigateAnimation.
            const float ease = progress * (progress - 2.0f);
            const ImVec2 dMin(
                m_NavTarget.Min.x - m_NavStart.Min.x,
                m_NavTarget.Min.y - m_NavStart.Min.y);
            const ImVec2 dMax(
                m_NavTarget.Max.x - m_NavStart.Max.x,
                m_NavTarget.Max.y - m_NavStart.Max.y);
            ImRect current;
            current.Min = ImVec2(m_NavStart.Min.x - dMin.x * ease, m_NavStart.Min.y - dMin.y * ease);
            current.Max = ImVec2(m_NavStart.Max.x - dMax.x * ease, m_NavStart.Max.y - dMax.y * ease);
            SetViewRect(current);
        }
        else
        {
            SetViewRect(m_NavTarget);
            CancelNavigationAnimation();
        }
    }

    void Widget::HandlePan()
    {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)
            && !(ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::GetIO().KeyAlt))
        {
            m_Mode = Mode::Idle;
            return;
        }

        // ax: Scroll = ScrollStart - dragDelta * Zoom (dragDelta is screen-space).
        const ImVec2 dragDelta = ImGui::GetMouseDragDelta(
            ImGui::IsMouseDown(ImGuiMouseButton_Middle) ? ImGuiMouseButton_Middle : ImGuiMouseButton_Left);
        m_Scroll = ImVec2(
            m_ScrollStart.x - dragDelta.x * m_Zoom,
            m_ScrollStart.y - dragDelta.y * m_Zoom);
        ApplyScrollZoomToView();
        m_Canvas.SetView(m_View);
    }

    void Widget::HandleZoom()
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (!ImGui::IsWindowHovered())
        {
            return;
        }

        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

        const float wheel = io.MouseWheel;
        if (wheel == 0.0f)
        {
            return;
        }

        // Port of ed::NavigateAction::HandleZoom (mouse-anchored + view-rect animation).
        const ImVec2 mousePos = ImGui::GetMousePos();
        const float savedZoom = m_Zoom;
        const ImVec2 savedScroll = m_Scroll;

        const ImGuiEx::CanvasView oldView(ImVec2(-m_Scroll.x, -m_Scroll.y), m_Zoom);
        const float newZoom = GetNextZoom(wheel);
        if (std::abs(newZoom - m_Zoom) < 1e-6f)
        {
            return;
        }

        m_Zoom = newZoom;
        const ImGuiEx::CanvasView newView(ImVec2(-m_Scroll.x, -m_Scroll.y), m_Zoom);

        const ImVec2 screenPos = m_Canvas.FromLocal(mousePos, oldView);
        const ImVec2 canvasPos = m_Canvas.ToLocal(screenPos, newView);
        const ImVec2 offset((canvasPos.x - mousePos.x) * m_Zoom, (canvasPos.y - mousePos.y) * m_Zoom);
        const ImVec2 targetScroll(m_Scroll.x - offset.x, m_Scroll.y - offset.y);

        // Restore then animate toward target (ax Finish/Stop leaves current, then NavigateTo).
        m_Zoom = savedZoom;
        m_Scroll = savedScroll;

        const ImRect targetRect =
            m_Canvas.CalcViewRect(ImGuiEx::CanvasView(ImVec2(-targetScroll.x, -targetScroll.y), newZoom));

        m_NavStart = GetViewRect();
        m_NavTarget = targetRect;
        m_NavTime = 0.0f;
        m_NavDuration = m_Style.ZoomAnimDuration;
        m_NavAnimating = true;

        if (m_NavDuration <= 0.0f)
        {
            SetViewRect(m_NavTarget);
            CancelNavigationAnimation();
        }
    }

    void Widget::HandleShortcuts(Document& document, std::vector<EditEvent>& outEvents)
    {
        if (m_Mode != Mode::Idle)
        {
            return;
        }

        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && !ImGui::IsWindowHovered())
        {
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && document.GetSelection().HasSelection())
        {
            EditEvent event;
            event.Kind = EditKind::DeleteSelectionRequested;
            event.Node = document.GetSelection().Node;
            event.Edge = document.GetSelection().Edge;
            outEvents.push_back(event);
        }
    }

    void Widget::HandleContextMenu(Document& document, std::vector<EditEvent>& outEvents)
    {
        (void)document;

        m_Canvas.Suspend();
        if (m_OpenContextMenu)
        {
            ImGui::OpenPopup("SmGraphBackgroundContext");
            m_OpenContextMenu = false;
        }

        if (ImGui::BeginPopup("SmGraphBackgroundContext"))
        {
            if (ImGui::MenuItem("Add State"))
            {
                EditEvent event;
                event.Kind = EditKind::AddNodeRequested;
                event.Pos = m_ContextMenuCanvasPos;
                outEvents.push_back(event);
            }
            ImGui::EndPopup();
        }
        m_Canvas.Resume();
    }

    ImVec2 Widget::NodeCenter(const Node& node)
    {
        return ImVec2(node.Pos.x + node.Size.x * 0.5f, node.Pos.y + node.Size.y * 0.5f);
    }

    ImVec2 Widget::ClosestPointOnRectBorder(const ImVec2& rectMin, const ImVec2& rectMax, const ImVec2& toward)
    {
        const ImVec2 center((rectMin.x + rectMax.x) * 0.5f, (rectMin.y + rectMax.y) * 0.5f);
        ImVec2 dir(toward.x - center.x, toward.y - center.y);
        if (dir.x == 0.0f && dir.y == 0.0f)
        {
            return ImVec2(rectMax.x, center.y);
        }

        const float halfW = (rectMax.x - rectMin.x) * 0.5f;
        const float halfH = (rectMax.y - rectMin.y) * 0.5f;
        const float scaleX = halfW / std::abs(dir.x);
        const float scaleY = halfH / std::abs(dir.y);
        const float scale = (std::abs(dir.x) < 1e-4f) ? scaleY
            : (std::abs(dir.y) < 1e-4f) ? scaleX
            : std::min(scaleX, scaleY);

        return ImVec2(center.x + dir.x * scale, center.y + dir.y * scale);
    }

    void Widget::DrawArrowHead(ImDrawList* drawList, const ImVec2& tip, const ImVec2& direction, ImU32 color)
    {
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length < 1e-3f)
        {
            return;
        }

        const ImVec2 dir(direction.x / length, direction.y / length);
        const ImVec2 ortho(-dir.y, dir.x);
        constexpr float kSize = 8.0f;
        const ImVec2 p1(
            tip.x - dir.x * kSize + ortho.x * (kSize * 0.5f),
            tip.y - dir.y * kSize + ortho.y * (kSize * 0.5f));
        const ImVec2 p2(
            tip.x - dir.x * kSize - ortho.x * (kSize * 0.5f),
            tip.y - dir.y * kSize - ortho.y * (kSize * 0.5f));
        drawList->AddTriangleFilled(tip, p1, p2, color);
    }

    float Widget::DistancePointToSegment(const ImVec2& point, const ImVec2& a, const ImVec2& b)
    {
        const ImVec2 ab(b.x - a.x, b.y - a.y);
        const float abLenSq = ab.x * ab.x + ab.y * ab.y;
        if (abLenSq < 1e-6f)
        {
            const ImVec2 ap(point.x - a.x, point.y - a.y);
            return std::sqrt(ap.x * ap.x + ap.y * ap.y);
        }

        const ImVec2 ap(point.x - a.x, point.y - a.y);
        float t = (ap.x * ab.x + ap.y * ab.y) / abLenSq;
        t = std::clamp(t, 0.0f, 1.0f);
        const ImVec2 closest(a.x + ab.x * t, a.y + ab.y * t);
        const ImVec2 diff(point.x - closest.x, point.y - closest.y);
        return std::sqrt(diff.x * diff.x + diff.y * diff.y);
    }
}
