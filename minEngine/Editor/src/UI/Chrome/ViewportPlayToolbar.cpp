#include "ViewportPlayToolbar.h"

#include <algorithm>

#include "PlayMode/IPlayModeService.h"
#include "Shell/IEditorContext.h"
#include "UI/Appearance/EditorAppearance.h"

#include "IconFontCppHeaders/IconsFontAwesome7.h"
#include "imgui.h"

namespace minEngine
{
    namespace
    {
        constexpr float kToolbarIconFontSize = 17.0f;
        constexpr float kToolbarButtonSize = 30.0f;
        constexpr float kToolbarVerticalPadding = 6.0f;
        constexpr float kToolbarItemSpacing = 8.0f;
        constexpr float kToolbarGroupSeparatorGap = 12.0f;

        bool DrawIconButton(const char* label, bool enabled, bool emphasizeStop = false)
        {
            ImGui::BeginDisabled(!enabled);

            if (emphasizeStop)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.18f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.24f, 0.24f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
            }

            const bool clicked = ImGui::Button(label, ImVec2(kToolbarButtonSize, kToolbarButtonSize));

            if (emphasizeStop)
            {
                ImGui::PopStyleColor(3);
            }

            ImGui::EndDisabled();
            return clicked && enabled;
        }

        float MeasureToolbarClusterWidth()
        {
            const float separatorWidth = ImGui::CalcTextSize("|").x + (kToolbarGroupSeparatorGap * 2.0f);
            return (kToolbarButtonSize * 4.0f) + (kToolbarItemSpacing * 3.0f) + separatorWidth;
        }

        void DrawToolbarButtons(IEditorContext& context, ImFont* iconFont)
        {
            IPlayModeService& playMode = context.GetPlayModeService();
            const bool isPlaying = playMode.IsPlaying();

            ImGui::PushFont(iconFont, kToolbarIconFontSize);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kToolbarItemSpacing, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

            if (DrawIconButton(ICON_FA_PLAY "##Play", !isPlaying))
            {
                playMode.EnterPlay();
            }
            ImGui::SameLine();
            if (DrawIconButton(ICON_FA_STOP "##Stop", isPlaying, isPlaying))
            {
                playMode.Stop();
            }

            ImGui::SameLine(0.0f, kToolbarGroupSeparatorGap);
            ImGui::TextUnformatted("|");
            ImGui::SameLine(0.0f, kToolbarGroupSeparatorGap);

            DrawIconButton(ICON_FA_PAUSE "##Pause", false);
            ImGui::SameLine();
            DrawIconButton(ICON_FA_FORWARD_STEP "##Step", false);

            ImGui::PopStyleVar(3);
            ImGui::PopFont();
        }
    }

    float ViewportPlayToolbar::DrawToolbarRow(IEditorContext& context)
    {
        EditorAppearance& appearance = context.GetEditorAppearance();
        ImFont* iconFont = appearance.GetAssetIconSolidImFont();
        if (iconFont == nullptr)
        {
            iconFont = appearance.GetAssetIconRegularImFont();
        }
        if (iconFont == nullptr)
        {
            return 0.0f;
        }

        const ImVec2 rowStart = ImGui::GetCursorPos();
        const float rowWidth = ImGui::GetContentRegionAvail().x;
        const float rowHeight = kToolbarButtonSize + (kToolbarVerticalPadding * 2.0f);
        const float clusterWidth = MeasureToolbarClusterWidth();
        const float horizontalOffset = std::max(0.0f, (rowWidth - clusterWidth) * 0.5f);

        ImGui::SetCursorPos(ImVec2(rowStart.x + horizontalOffset, rowStart.y + kToolbarVerticalPadding));
        DrawToolbarButtons(context, iconFont);

        ImGui::SetCursorPos(ImVec2(rowStart.x, rowStart.y + rowHeight));
        ImGui::Separator();

        const float separatorSpacing = ImGui::GetStyle().ItemSpacing.y;
        return rowHeight + separatorSpacing + 1.0f;
    }
}
