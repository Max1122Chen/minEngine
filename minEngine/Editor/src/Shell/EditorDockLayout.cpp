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
        }

        void BuildSceneEditingLayout(ImGuiID dockspaceId)
        {
            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

            ImGuiID mainArea = dockspaceId;
            ImGuiID inspectorArea = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Right, kInspectorSplitRatio, nullptr, &mainArea);
            ImGuiID hierarchyArea = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Right, kHierarchySplitRatio, nullptr, &mainArea);
            ImGuiID consoleArea = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Down, kConsoleSplitRatio, nullptr, &mainArea);

            ImGui::DockBuilderDockWindow("Viewport", mainArea);
            ImGui::DockBuilderDockWindow("Console", consoleArea);
            ImGui::DockBuilderDockWindow("Hierarchy", hierarchyArea);
            ImGui::DockBuilderDockWindow("Inspector", inspectorArea);

            ImGui::DockBuilderFinish(dockspaceId);
        }

        void BuildMaterialEditingLayout(ImGuiID dockspaceId)
        {
            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

            ImGuiID mainArea = dockspaceId;
            ImGuiID inspectorArea = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Right, kInspectorSplitRatio, nullptr, &mainArea);
            ImGuiID consoleArea = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Down, kConsoleSplitRatio, nullptr, &mainArea);
            ImGuiID graphArea = ImGui::DockBuilderSplitNode(
                mainArea, ImGuiDir_Right, 0.58f, nullptr, &mainArea);

            ImGui::DockBuilderDockWindow("Material Preview", mainArea);
            ImGui::DockBuilderDockWindow("Material Graph", graphArea);
            ImGui::DockBuilderDockWindow("Inspector", inspectorArea);
            ImGui::DockBuilderDockWindow("Console", consoleArea);

            ImGui::DockBuilderFinish(dockspaceId);
        }
    }
}
