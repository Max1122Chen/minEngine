#include "UI/Inspector/InspectorPreviewPresenter.h"

#include "Services/Thumbnail/AssetThumbnailService.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include "imgui.h"

#include <algorithm>

namespace minEngine
{
    void InspectorPreviewPresenter::DrawSquarePreviewSlot(
        IEditorContext& context,
        AssetThumbnailService& thumbnailService)
    {
        (void)context;

        if (!thumbnailService.HasPreviewContent())
        {
            return;
        }

        const float contentWidth = ImGui::GetContentRegionAvail().x;
        const float squareSize = std::min(contentWidth, kMaxSquareSize);
        if (squareSize <= 0.0f)
        {
            return;
        }

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

        const ThumbnailView view = thumbnailService.RequestThumbnail();
        if (view.State != ThumbnailState::Ready || view.TextureId == 0
            || view.Width == 0 || view.Height == 0)
        {
            ImGui::TextWrapped("Preview is not ready.");
            return;
        }

        const uint32_t textureWidth = view.Width;
        const uint32_t textureHeight = view.Height;
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

        drawCenteredImage(imageSize, view.TextureId);
    }
}
