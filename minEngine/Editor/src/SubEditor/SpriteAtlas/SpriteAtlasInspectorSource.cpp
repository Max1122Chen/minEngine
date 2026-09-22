#include "SpriteAtlasInspectorSource.h"

#include "SpriteAtlasEditor.h"

#include "imgui.h"

#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    SpriteAtlasInspectorSource::SpriteAtlasInspectorSource(SpriteAtlasEditor& owner)
        : m_Owner(owner)
    {
    }

    bool SpriteAtlasInspectorSource::HasInspectableSelection() const
    {
        return m_Owner.GetSession().HasOpenAtlas();
    }

    void SpriteAtlasInspectorSource::DrawInspector()
    {
        const SpriteAtlasEditorSession& session = m_Owner.GetSession();
        if (!session.HasOpenAtlas())
        {
            ImGui::TextUnformatted("No Sprite Atlas open.");
            return;
        }

        SpriteAtlas& atlas = *session.AtlasAsset;
        ImGui::Text("Asset: %s", session.AssetPath.c_str());
        ImGui::Text("Members: %zu", atlas.GetMembers().size());
        ImGui::Text("MaxSize: %d  Padding: %d", atlas.GetMaxSize(), atlas.GetPadding());
        ImGui::Text("Generated pages: %zu", atlas.GetGeneratedTextures().size());
        if (!session.LastRebuildMessage.empty())
        {
            ImGui::TextWrapped("%s", session.LastRebuildMessage.c_str());
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Edit members and Rebuild in the Sprite Atlas Editor panel.");
    }
}
