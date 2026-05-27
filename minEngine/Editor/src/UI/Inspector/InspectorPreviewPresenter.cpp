#include "UI/Inspector/InspectorPreviewPresenter.h"

#include "Services/Inspector/InspectorAssetInspection.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/Texture.h"

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

        const PreviewDisplayKind displayKind = inspection.GetDisplayKind();

        auto drawCenteredImage =
            [&](const ImVec2& imageSize, const ImTextureID textureId)
        {
            const float centerOffsetX = std::max(0.0f, (contentWidth - imageSize.x) * 0.5f);
            if (centerOffsetX > 0.0f)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffsetX);
            }

            ImGui::Image(textureId, imageSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        };

        if (displayKind == PreviewDisplayKind::Scene3D)
        {
            const uint32_t renderSize = static_cast<uint32_t>(squareSize);
            inspection.RenderInspection(renderSize);

            const std::shared_ptr<RHITexture2D>& displayTexture = inspection.GetSceneColorTexture();
            if (!displayTexture || displayTexture->GetWidth() == 0 || displayTexture->GetHeight() == 0)
            {
                ImGui::TextWrapped("Preview is not ready.");
                return;
            }

            const ImVec2 imageSize(squareSize, squareSize);
            const ImTextureID textureId =
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(displayTexture->GetID()));
            drawCenteredImage(imageSize, textureId);
            return;
        }

        if (displayKind != PreviewDisplayKind::Texture2DImage)
        {
            return;
        }

        const Texture2D* textureAsset = inspection.GetTexture2DPreviewAsset();
        if (!textureAsset)
        {
            ImGui::TextWrapped("Failed to load texture.");
            return;
        }

        const RHITexture2D* rhiTexture = textureAsset->GetRHITexture();
        if (!rhiTexture || rhiTexture->GetID() == 0)
        {
            ImGui::TextWrapped("Texture preview is not available.");
            return;
        }

        uint32_t textureWidth = rhiTexture->GetWidth();
        uint32_t textureHeight = rhiTexture->GetHeight();
        if (textureWidth == 0 || textureHeight == 0)
        {
            textureWidth = textureAsset->GetWidth();
            textureHeight = textureAsset->GetHeight();
        }

        if (textureWidth == 0 || textureHeight == 0)
        {
            ImGui::TextWrapped("Texture preview is not available.");
            return;
        }

        const float textureAspect =
            static_cast<float>(textureWidth) / static_cast<float>(textureHeight);
        ImVec2 imageSize(squareSize, squareSize);
        if (textureAspect >= 1.0f)
        {
            imageSize.y = squareSize / textureAspect;
        }
        else
        {
            imageSize.x = squareSize * textureAspect;
        }

        const ImTextureID textureId =
            reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(rhiTexture->GetID()));
        drawCenteredImage(imageSize, textureId);
    }
}
