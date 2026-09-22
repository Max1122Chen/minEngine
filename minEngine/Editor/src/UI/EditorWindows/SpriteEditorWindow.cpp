#include "SpriteEditorWindow.h"

#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "Shell/EditorContextHelpers.h"
#include "SubEditor/Sprite/SpriteEditor.h"

#include "imgui.h"

#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Function/Render/Texture.h"

#include <algorithm>
#include <cmath>

namespace minEngine
{
    SpriteEditorWindow::SpriteEditorWindow(IEditorContext& context)
        : EditorWindow(context)
    {
        SetOpen(true);
    }

    std::string_view SpriteEditorWindow::GetOwnerModuleId() const
    {
        return SpriteEditor::kModuleId;
    }

    void SpriteEditorWindow::OnDraw()
    {
        if (!EditorWindowTypography::BeginPanel(m_Context, m_Title.c_str(), nullptr, 0))
        {
            return;
        }

        EditorTypographyScope bodyTypography(m_Context.GetEditorAppearance(), EditorTypographyRole::Body);
        DrawPreviewAndProperties();
        ImGui::End();
    }

    void SpriteEditorWindow::DrawPreviewAndProperties()
    {
        SpriteEditor* editor = GetSpriteEditor(&m_Context);
        if (editor == nullptr)
        {
            ImGui::TextUnformatted("Sprite editor module unavailable.");
            return;
        }

        SpriteEditorSession& session = editor->GetSession();
        if (!session.HasOpenSprite())
        {
            ImGui::TextUnformatted("Open a .mesprite asset from Content Browser.");
            return;
        }

        Sprite& sprite = *session.SpriteAsset;
        Texture2D* sourceTexture = sprite.GetSourceTexture();
        const bool canSave = sourceTexture != nullptr;

        if (ImGui::Button("Save") && canSave)
        {
            editor->SaveActiveSprite();
        }
        if (!canSave)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "SourceTexture missing — Save disabled");
        }
        else if (session.Dirty)
        {
            ImGui::SameLine();
            ImGui::TextUnformatted("* Dirty");
        }

        if (sprite.GetBakedTexture() != nullptr)
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
                "Baked UV present. Changing SourceRect requires Atlas Rebuild (F06).");
        }
        if (SpriteAtlas* atlas = sprite.GetAtlas())
        {
            ImGui::Text("Atlas owner: %s", atlas->GetName().c_str());
        }

        ImGui::Separator();

        Vector4 rect = sprite.ResolveSourceRectPx();
        int rectInts[4] = {
            static_cast<int>(std::lround(rect.x)),
            static_cast<int>(std::lround(rect.y)),
            static_cast<int>(std::lround(rect.z)),
            static_cast<int>(std::lround(rect.w)),
        };

        bool rectEdited = ImGui::DragInt4("SourceRect (x,y,w,h)", rectInts, 1.0f, 0, 8192);
        if (rectEdited)
        {
            rectInts[2] = std::max(1, rectInts[2]);
            rectInts[3] = std::max(1, rectInts[3]);
            editor->SubmitSetSourceRect(Vector4(
                static_cast<float>(rectInts[0]),
                static_cast<float>(rectInts[1]),
                static_cast<float>(rectInts[2]),
                static_cast<float>(rectInts[3])));
            rect = sprite.ResolveSourceRectPx();
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Source Preview (drag to set rect)");

        if (sourceTexture == nullptr || sourceTexture->GetRHITexture() == nullptr)
        {
            ImGui::TextUnformatted("(no preview)");
            return;
        }

        if (!RenderSystem::HasInstance() || RenderSystem::Get().GetRHI() == nullptr)
        {
            ImGui::TextUnformatted("(RHI unavailable)");
            return;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        const ImTextureID textureId = m_PinnedSourceTexture.Pin(rhi, sourceTexture->GetRHITexture());
        if (textureId == ImTextureID_Invalid)
        {
            ImGui::TextUnformatted("(failed to pin texture for ImGui)");
            return;
        }

        const float texW = static_cast<float>(std::max(1u, sourceTexture->GetWidth()));
        const float texH = static_cast<float>(std::max(1u, sourceTexture->GetHeight()));
        const float availW = std::max(64.0f, ImGui::GetContentRegionAvail().x);
        const float scale = std::min(1.0f, availW / texW);
        const ImVec2 imageSize(texW * scale, texH * scale);

        const ImVec2 imagePos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##SpritePreview", imageSize);
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        // OpenGL textures: v=0 at bottom → flip for top-left authoring preview.
        drawList->AddImage(
            textureId,
            imagePos,
            ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y),
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f));

        auto ImageToPixel = [&](const ImVec2& mouse) -> ImVec2
        {
            const float px = std::clamp((mouse.x - imagePos.x) / scale, 0.0f, texW);
            const float py = std::clamp((mouse.y - imagePos.y) / scale, 0.0f, texH);
            return ImVec2(px, py);
        };

        auto PixelToScreen = [&](float px, float py) -> ImVec2
        {
            return ImVec2(imagePos.x + px * scale, imagePos.y + py * scale);
        };

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_IsDraggingRect = true;
            m_DragStartImagePx = ImageToPixel(ImGui::GetIO().MousePos);
            m_DragRectPx = Vector4(m_DragStartImagePx.x, m_DragStartImagePx.y, 1.0f, 1.0f);
        }

        if (m_IsDraggingRect && active)
        {
            const ImVec2 current = ImageToPixel(ImGui::GetIO().MousePos);
            const float x0 = std::min(m_DragStartImagePx.x, current.x);
            const float y0 = std::min(m_DragStartImagePx.y, current.y);
            const float x1 = std::max(m_DragStartImagePx.x, current.x);
            const float y1 = std::max(m_DragStartImagePx.y, current.y);
            m_DragRectPx = Vector4(x0, y0, std::max(1.0f, x1 - x0), std::max(1.0f, y1 - y0));
        }

        if (m_IsDraggingRect && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            editor->SubmitSetSourceRect(m_DragRectPx);
            rect = sprite.ResolveSourceRectPx();
            m_IsDraggingRect = false;
        }

        const Vector4 drawRect = m_IsDraggingRect ? m_DragRectPx : rect;
        const ImVec2 rectMin = PixelToScreen(drawRect.x, drawRect.y);
        const ImVec2 rectMax = PixelToScreen(drawRect.x + drawRect.z, drawRect.y + drawRect.w);

        // Dim outside selection.
        const ImU32 dimColor = IM_COL32(0, 0, 0, 120);
        const ImVec2 imageMax(imagePos.x + imageSize.x, imagePos.y + imageSize.y);
        drawList->AddRectFilled(imagePos, ImVec2(imageMax.x, rectMin.y), dimColor);
        drawList->AddRectFilled(ImVec2(imagePos.x, rectMax.y), imageMax, dimColor);
        drawList->AddRectFilled(ImVec2(imagePos.x, rectMin.y), ImVec2(rectMin.x, rectMax.y), dimColor);
        drawList->AddRectFilled(ImVec2(rectMax.x, rectMin.y), ImVec2(imageMax.x, rectMax.y), dimColor);
        drawList->AddRect(rectMin, rectMax, IM_COL32(80, 200, 255, 255), 0.0f, 0, 2.0f);
    }
}
