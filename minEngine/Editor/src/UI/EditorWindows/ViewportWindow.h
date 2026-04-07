#pragma once
#include "Core.h"

#include "imgui.h"

#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"

#include "UI/Widgets/DraggableOverlay.h"

#include "Editor.h"
#include "EditorWindow.h"

#include <algorithm>

namespace minEngine
{
    class ViewportWindow final : public EditorWindow
    {
    public:
        explicit ViewportWindow(Editor& editor)
            : EditorWindow(editor)
        {
        }

        const std::string& GetId() const override
        {
            return m_Id;
        }

        const std::string& GetTitle() const override
        {
            return m_Title;
        }

        void OnDraw() override
        {
            ImGuiWindowFlags viewportFlags = ImGuiWindowFlags_NoScrollbar |
                                             ImGuiWindowFlags_NoScrollWithMouse |
                                             ImGuiWindowFlags_NoCollapse;

            ImGui::Begin(m_Title.c_str(), nullptr, viewportFlags);

            ImVec2 avail = ImGui::GetContentRegionAvail();

            const auto& renderSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem;
            if (!renderSystem)
            {
                ImGui::TextWrapped("RenderSystem is not ready.");
                ImGui::End();
                return;
            }

            const std::shared_ptr<RHITexture2D>& sceneColor = renderSystem->GetSceneColorTexture();
            if (!sceneColor)
            {
                ImGui::TextWrapped("Scene color texture is not ready.");
                ImGui::End();
                return;
            }

            const ImTextureID textureID = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(sceneColor->GetID()));
            ImGui::Image(textureID, avail, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            const ImVec2 imageMin = ImGui::GetItemRectMin();
            const ImVec2 imageSize = ImGui::GetItemRectSize();

            m_Editor.viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
            m_Editor.viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

            const float deltaTime = m_Editor.lastDeltaTime;
            const float fps = (deltaTime > 0.0001f) ? (1.0f / deltaTime) : 0.0f;
            const std::string sceneName = m_Editor.GetCurrentScenePath().filename().string().empty()
                ? std::string("Untitled")
                : m_Editor.GetCurrentScenePath().filename().string();

            m_OverlayConfig.expandedSize = ImVec2(std::max(220.0f, std::min(420.0f, imageSize.x * 0.46f)), 96.0f);
            UI::ClampOverlayOffset(m_OverlayState, m_OverlayConfig, imageSize);
            {
                ImGui::SetCursorScreenPos(UI::GetOverlayScreenPos(m_OverlayState, imageMin));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.10f, 0.13f, 0.82f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.27f, 0.37f, 0.50f, 0.95f));
                if (ImGui::BeginChild("ViewportOverlay", UI::GetOverlaySize(m_OverlayState, m_OverlayConfig), true,
                                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav))
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));
                    if (m_OverlayState.collapsed)
                    {
                        if (ImGui::Button(">"))
                        {
                            m_OverlayState.collapsed = false;
                        }

                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("Expand overlay");
                        }
                    }
                    else
                    {
                        ImGui::TextUnformatted("Overlay");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 24.0f);
                        if (ImGui::Button("_"))
                        {
                            m_OverlayState.collapsed = true;
                        }

                        ImGui::Separator();
                        ImGui::TextWrapped("Scene: %s", sceneName.c_str());
                        ImGui::Text("FPS: %.1f", fps);
                        ImGui::Text("Frame: %.2f ms", deltaTime * 1000.0f);
                        ImGui::Text("Viewport: %.0f x %.0f", avail.x, avail.y);
                    }

                    UI::HandleOverlayDragging(m_OverlayState, m_OverlayConfig, imageSize);
                    ImGui::PopStyleVar();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor(2);
            }

            ImGui::End();
        }

    private:
        const std::string m_Id = "viewport";
        const std::string m_Title = "Viewport";
        UI::DraggableOverlayState m_OverlayState;
        UI::DraggableOverlayConfig m_OverlayConfig;
    };
}
