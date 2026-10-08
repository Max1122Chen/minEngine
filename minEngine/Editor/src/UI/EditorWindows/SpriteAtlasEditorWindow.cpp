#include "SpriteAtlasEditorWindow.h"

#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "Shell/EditorContextHelpers.h"
#include "SubEditor/SpriteAtlas/SpriteAtlasEditor.h"

#include "imgui.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include <algorithm>
#include <string>
#include <vector>

namespace minEngine
{
    SpriteAtlasEditorWindow::SpriteAtlasEditorWindow(IEditorContext& context)
        : EditorWindow(context)
    {
        SetOpen(true);
    }

    std::string_view SpriteAtlasEditorWindow::GetOwnerModuleId() const
    {
        return SpriteAtlasEditor::kModuleId;
    }

    void SpriteAtlasEditorWindow::OnDraw()
    {
        if (!EditorWindowTypography::BeginPanel(m_Context, m_Title.c_str(), nullptr, 0))
        {
            return;
        }

        EditorTypographyScope bodyTypography(m_Context.GetEditorAppearance(), EditorTypographyRole::Body);
        DrawMembersAndControls();
        ImGui::Separator();
        DrawGeneratedPreview();
        ImGui::End();
    }

    void SpriteAtlasEditorWindow::DrawMembersAndControls()
    {
        SpriteAtlasEditor* editor = GetSpriteAtlasEditor(&m_Context);
        if (editor == nullptr)
        {
            ImGui::TextUnformatted("Sprite Atlas editor module unavailable.");
            return;
        }

        SpriteAtlasEditorSession& session = editor->GetSession();
        if (!session.HasOpenAtlas())
        {
            ImGui::TextUnformatted("Open a .mespriteatlas asset from Content Browser.");
            return;
        }

        SpriteAtlas& atlas = *session.AtlasAsset;

        if (ImGui::Button("Save"))
        {
            editor->SaveActiveAtlas();
        }
        if (session.Dirty)
        {
            ImGui::SameLine();
            ImGui::TextUnformatted("* Dirty");
        }
        ImGui::SameLine();
        if (ImGui::Button("Rebuild"))
        {
            editor->SubmitRebuild();
        }

        if (!session.LastRebuildMessage.empty())
        {
            ImGui::TextWrapped("%s", session.LastRebuildMessage.c_str());
        }

        int maxSize = atlas.GetMaxSize();
        int padding = atlas.GetPadding();
        if (ImGui::DragInt("MaxSize", &maxSize, 1.0f, 16, 8192))
        {
            editor->SetMaxSize(maxSize);
        }
        if (ImGui::DragInt("Padding", &padding, 1.0f, 0, 64))
        {
            editor->SetPadding(padding);
        }

        int paddingMode = static_cast<int>(atlas.GetPaddingMode());
        const char* paddingModeItems[] = {"PadWithZero", "DilateBorder"};
        if (ImGui::Combo(
                "PaddingMode",
                &paddingMode,
                paddingModeItems,
                static_cast<int>(IM_ARRAYSIZE(paddingModeItems))))
        {
            editor->SetPaddingMode(static_cast<SpriteAtlasPaddingMode>(paddingMode));
        }

        ImGui::Separator();
        ImGui::Text("Members (%zu)", atlas.GetMembers().size());

        const std::vector<std::shared_ptr<Sprite>>& members = atlas.GetMembers();
        for (size_t i = 0; i < members.size(); ++i)
        {
            const std::shared_ptr<Sprite>& sprite = members[i];
            if (!sprite)
            {
                continue;
            }

            ImGui::PushID(static_cast<int>(i));
            const bool baked = sprite->GetBakedTexture() != nullptr;
            ImGui::Text("%s%s", sprite->GetName().c_str(), baked ? " [Baked]" : " [Source]");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove"))
            {
                editor->SubmitRemoveMember(sprite);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        if (!AssetManager::HasInstance())
        {
            return;
        }

        std::vector<const AssetMeta*> spriteMetas = AssetManager::Get().FindAssetMetasByType("Sprite");
        std::sort(
            spriteMetas.begin(),
            spriteMetas.end(),
            [](const AssetMeta* a, const AssetMeta* b)
            {
                return a->AssetPath < b->AssetPath;
            });

        std::vector<std::string> labels;
        labels.reserve(spriteMetas.size());
        for (const AssetMeta* meta : spriteMetas)
        {
            labels.push_back(meta != nullptr ? meta->AssetPath : std::string("(null)"));
        }

        if (ImGui::BeginCombo("Add Sprite", "Select Sprite..."))
        {
            for (size_t i = 0; i < spriteMetas.size(); ++i)
            {
                const bool selected = static_cast<int>(i) == m_AddSpritePickerIndex;
                if (ImGui::Selectable(labels[i].c_str(), selected))
                {
                    m_AddSpritePickerIndex = static_cast<int>(i);
                    std::shared_ptr<Sprite> sprite =
                        ([&]() -> std::shared_ptr<Sprite> { std::string _loadAssetError; return std::dynamic_pointer_cast<Sprite>(AssetManager::Get().LoadAssetByPath(spriteMetas[i]->AssetPath, _loadAssetError)); })();
                    if (sprite)
                    {
                        editor->SubmitAddMembers({ sprite });
                    }
                }
            }
            ImGui::EndCombo();
        }
    }

    void SpriteAtlasEditorWindow::DrawGeneratedPreview()
    {
        SpriteAtlasEditor* editor = GetSpriteAtlasEditor(&m_Context);
        if (editor == nullptr || !editor->GetSession().HasOpenAtlas())
        {
            return;
        }

        SpriteAtlas& atlas = *editor->GetSession().AtlasAsset;
        const std::vector<std::shared_ptr<Texture2D>>& generated = atlas.GetGeneratedTextures();
        ImGui::TextUnformatted("Generated Preview (read-only)");
        if (generated.empty() || !generated[0] || generated[0]->GetRHITexture() == nullptr)
        {
            ImGui::TextUnformatted("(Rebuild to generate atlas texture)");
            return;
        }

        if (!RenderSystem::HasInstance() || RenderSystem::Get().GetRHI() == nullptr)
        {
            ImGui::TextUnformatted("(RHI unavailable)");
            return;
        }

        Texture2D& texture = *generated[0];
        RHI* rhi = RenderSystem::Get().GetRHI();
        const ImTextureID textureId = m_PinnedGeneratedTexture.Pin(rhi, texture.GetRHITexture());
        if (textureId == ImTextureID_Invalid)
        {
            ImGui::TextUnformatted("(failed to pin generated texture)");
            return;
        }

        const float texW = static_cast<float>(std::max(1u, texture.GetWidth()));
        const float texH = static_cast<float>(std::max(1u, texture.GetHeight()));
        const float availW = std::max(64.0f, ImGui::GetContentRegionAvail().x);
        const float scale = std::min(1.0f, availW / texW);
        const ImVec2 imageSize(texW * scale, texH * scale);
        ImGui::Image(textureId, imageSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        ImGui::Text("%dx%d", texture.GetWidth(), texture.GetHeight());
    }
}
