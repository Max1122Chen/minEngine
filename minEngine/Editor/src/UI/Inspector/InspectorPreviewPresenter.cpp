#include "UI/Inspector/InspectorPreviewPresenter.h"

#include "Services/Inspector/InspectorAssetInspection.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include "imgui.h"

#include <algorithm>

namespace minEngine
{
    void InspectorPreviewPresenter::DrawSquarePreviewSlot(
        IEditorContext& context,
        InspectorAssetInspection& inspection)
    {
        (void)context;

        if (!inspection.HasPreviewContent())
        {
            return;
        }

        const float contentWidth = ImGui::GetContentRegionAvail().x;
        const float squareSize = std::min(contentWidth, kMaxSquareSize);
        if (squareSize <= 0.0f)
        {
            return;
        }

        if (inspection.GetDisplayKind() != PreviewDisplayKind::Scene3D)
        {
            return;
        }

        const uint32_t renderSize = static_cast<uint32_t>(squareSize);
        inspection.RenderInspection(renderSize);

        const std::shared_ptr<RHITexture2D>& displayTexture = inspection.GetSceneColorTexture();
        if (!displayTexture || displayTexture->GetWidth() == 0 || displayTexture->GetHeight() == 0)
        {
            ImGui::TextWrapped("Preview is not ready.");
            return;
        }

        const float centerOffsetX = std::max(0.0f, (contentWidth - squareSize) * 0.5f);
        if (centerOffsetX > 0.0f)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffsetX);
        }

        const ImVec2 imageSize(squareSize, squareSize);
        const ImTextureID textureId =
            reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(displayTexture->GetID()));
        ImGui::Image(textureId, imageSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    }
}
