#pragma once

#include "imgui.h"

#include <algorithm>

namespace minEngine::UI
{
    struct DraggableOverlayState
    {
        bool collapsed = false;
        ImVec2 offset = ImVec2(12.0f, 12.0f);
    };

    struct DraggableOverlayConfig
    {
        ImVec2 expandedSize = ImVec2(280.0f, 96.0f);
        ImVec2 collapsedSize = ImVec2(32.0f, 24.0f);
        float padding = 8.0f;
    };

    inline ImVec2 GetOverlaySize(const DraggableOverlayState& state, const DraggableOverlayConfig& config)
    {
        return state.collapsed ? config.collapsedSize : config.expandedSize;
    }

    inline void ClampOverlayOffset(DraggableOverlayState& state,
                                   const DraggableOverlayConfig& config,
                                   const ImVec2& parentSize)
    {
        const ImVec2 overlaySize = GetOverlaySize(state, config);
        const float minPadding = config.padding;
        const float maxOffsetX = std::max(minPadding, parentSize.x - overlaySize.x - minPadding);
        const float maxOffsetY = std::max(minPadding, parentSize.y - overlaySize.y - minPadding);

        state.offset.x = std::clamp(state.offset.x, minPadding, maxOffsetX);
        state.offset.y = std::clamp(state.offset.y, minPadding, maxOffsetY);
    }

    inline ImVec2 GetOverlayScreenPos(const DraggableOverlayState& state, const ImVec2& parentMin)
    {
        return ImVec2(parentMin.x + state.offset.x, parentMin.y + state.offset.y);
    }

    inline void HandleOverlayDragging(DraggableOverlayState& state,
                                      const DraggableOverlayConfig& config,
                                      const ImVec2& parentSize)
    {
        if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
            || !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            return;
        }

        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        state.offset.x += delta.x;
        state.offset.y += delta.y;
        ClampOverlayOffset(state, config, parentSize);
    }
}
