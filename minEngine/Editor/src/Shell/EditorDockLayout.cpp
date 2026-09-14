#include "Shell/EditorDockLayout.h"

#include "imgui_internal.h"

namespace minEngine
{
    namespace EditorDockLayout
    {
        namespace
        {
            constexpr float kInspectorSplitRatio = 0.22f;
            constexpr float kHierarchySplitRatio = 0.28f;
            constexpr float kConsoleSplitRatio = 0.30f;
            constexpr float kRightBottomSplitRatio = 0.45f;
        }

        void BuildSceneEditingLayout(ImGuiID dockspaceId)
        {
            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

            ImGuiID mainArea = dockspaceId;
            ImGuiID rightColumn = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Right, kHierarchySplitRatio + kInspectorSplitRatio, nullptr, &mainArea);
            ImGuiID consoleArea = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Down, kConsoleSplitRatio, nullptr, &mainArea);

            // Shared: Content Browser spans the full right column (below Hierarchy + Inspector).
            ImGuiID rightColumnTop = rightColumn;
            ImGuiID contentBrowserArea = ImGui::DockBuilderSplitNode(
                rightColumn, ImGuiDir_Down, kRightBottomSplitRatio, nullptr, &rightColumnTop);

            ImGuiID inspectorArea = ImGui::DockBuilderSplitNode(
                rightColumnTop,
                ImGuiDir_Right,
                kInspectorSplitRatio / (kHierarchySplitRatio + kInspectorSplitRatio),
                nullptr,
                &rightColumnTop);
            ImGuiID hierarchyArea = rightColumnTop;

            ImGui::DockBuilderDockWindow("Viewport", mainArea);
            ImGui::DockBuilderDockWindow("Console", consoleArea);
            ImGui::DockBuilderDockWindow("Hierarchy", hierarchyArea);
            ImGui::DockBuilderDockWindow("Inspector", inspectorArea);
            ImGui::DockBuilderDockWindow("Content Browser", contentBrowserArea);

            ImGui::DockBuilderFinish(dockspaceId);
        }

        void BuildMaterialEditingLayout(ImGuiID dockspaceId)
        {
            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

            ImGuiID mainArea = dockspaceId;
            ImGuiID rightColumn = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Right, kInspectorSplitRatio, nullptr, &mainArea);
            ImGuiID consoleArea = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Down, kConsoleSplitRatio, nullptr, &mainArea);
            ImGuiID graphArea = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Right, 0.58f, nullptr, &mainArea);

            ImGuiID rightColumnTop = rightColumn;
            ImGuiID contentBrowserArea = ImGui::DockBuilderSplitNode(
                rightColumn, ImGuiDir_Down, kRightBottomSplitRatio, nullptr, &rightColumnTop);
            ImGuiID inspectorArea = rightColumnTop;

            ImGui::DockBuilderDockWindow("Material Editor Viewport", mainArea);
            ImGui::DockBuilderDockWindow("Material Graph", graphArea);
            ImGui::DockBuilderDockWindow("Inspector", inspectorArea);
            ImGui::DockBuilderDockWindow("Content Browser", contentBrowserArea);
            ImGui::DockBuilderDockWindow("Console", consoleArea);

            ImGui::DockBuilderFinish(dockspaceId);
        }

        void BuildAnimationGraphEditingLayout(ImGuiID dockspaceId)
        {
            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

            ImGuiID mainArea = dockspaceId;
            ImGuiID rightColumn = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Right, 0.35f, nullptr, &mainArea);
            ImGuiID consoleArea = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Down, kConsoleSplitRatio, nullptr, &mainArea);

            ImGuiID rightColumnTop = rightColumn;
            ImGuiID contentBrowserArea = ImGui::DockBuilderSplitNode(
                rightColumn, ImGuiDir_Down, kRightBottomSplitRatio, nullptr, &rightColumnTop);

            ImGuiID detailsArea = rightColumnTop;
            ImGuiID parametersArea = ImGui::DockBuilderSplitNode(
                rightColumnTop, ImGuiDir_Down, 0.45f, nullptr, &detailsArea);

            ImGui::DockBuilderDockWindow("Anim Graph", mainArea);
            ImGui::DockBuilderDockWindow("Inspector", detailsArea);
            ImGui::DockBuilderDockWindow("Anim Graph Parameters", parametersArea);
            ImGui::DockBuilderDockWindow("Content Browser", contentBrowserArea);
            ImGui::DockBuilderDockWindow("Console", consoleArea);

            ImGui::DockBuilderFinish(dockspaceId);
        }
    }
}
