#include "SpriteInspectorSource.h"

#include "SpriteEditor.h"

#include "imgui.h"

#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    SpriteInspectorSource::SpriteInspectorSource(SpriteEditor& owner)
        : m_Owner(owner)
    {
    }

    bool SpriteInspectorSource::HasInspectableSelection() const
    {
        return m_Owner.GetSession().HasOpenSprite();
    }

    void SpriteInspectorSource::DrawInspector()
    {
        const SpriteEditorSession& session = m_Owner.GetSession();
        if (!session.HasOpenSprite())
        {
            ImGui::TextUnformatted("No Sprite open.");
            return;
        }

        Sprite& sprite = *session.SpriteAsset;
        ImGui::Text("Asset: %s", session.AssetPath.c_str());
        ImGui::Text("Name: %s", sprite.GetName().c_str());

        Texture2D* source = sprite.GetSourceTexture();
        if (source != nullptr)
        {
            ImGui::Text("Source: %dx%d", source->GetWidth(), source->GetHeight());
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "SourceTexture missing");
        }

        const Vector4 rect = sprite.GetSourceRectPx();
        ImGui::Text("SourceRect: (%.0f, %.0f, %.0f, %.0f)", rect.x, rect.y, rect.z, rect.w);

        if (sprite.GetBakedTexture() != nullptr)
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
                "Baked present — Rebuild Atlas after SourceRect changes.");
        }

        if (SpriteAtlas* atlas = sprite.GetAtlas())
        {
            ImGui::Text("Atlas: %s", atlas->GetName().c_str());
        }
        else
        {
            ImGui::TextUnformatted("Atlas: (none)");
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Edit SourceRect in the Sprite Editor panel.");
    }
}
