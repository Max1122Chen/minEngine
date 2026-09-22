#include "SpriteAtlasEditor.h"

#include "EditorGUIManager.h"
#include "Services/Sprite/SpriteEditorServices.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/EditorDockLayout.h"
#include "Shell/EditorInputHub.h"
#include "Shell/IEditorContext.h"
#include "UI/EditorWindows/SpriteAtlasEditorWindow.h"

#include "imgui.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

namespace minEngine
{
    SpriteAtlasEditor::SpriteAtlasEditor()
        : m_InspectorSource(*this)
    {
    }

    void SpriteAtlasEditor::Register(IEditorContext& context)
    {
        m_Context = &context;
        context.GetGUIManager().RegisterWindow(std::make_unique<SpriteAtlasEditorWindow>(context));
    }

    void SpriteAtlasEditor::Shutdown()
    {
        m_Session.Clear();
        m_SessionsByKey.clear();
        m_Context = nullptr;
    }

    void SpriteAtlasEditor::OnActivate(IEditorContext& context)
    {
        (void)context;
    }

    void SpriteAtlasEditor::OnDeactivate(IEditorContext& context)
    {
        (void)context;
        StashActiveSession();
    }

    void SpriteAtlasEditor::RegisterCommands(IEditorContext& context)
    {
        EditorCommandBinding saveCommand;
        saveCommand.Name = "Save Sprite Atlas";
        saveCommand.Chord = { ImGuiKey_S, true, false, false };
        saveCommand.CanExecute = [this]() { return m_Session.HasOpenAtlas(); };
        saveCommand.Execute = [this]() { SaveActiveAtlas(); };
        context.GetInputHub().RegisterActiveSubModuleCommand(std::move(saveCommand));
    }

    void SpriteAtlasEditor::UnregisterCommands(IEditorContext& context)
    {
        context.GetInputHub().ClearActiveSubModuleCommands();
    }

    void SpriteAtlasEditor::ApplyDefaultLayout(IEditorContext& context, ImGuiID dockspaceId)
    {
        (void)context;
        EditorDockLayout::BuildSpriteAtlasEditingLayout(dockspaceId);
    }

    bool SpriteAtlasEditor::CanOpenAsset(const AssetMeta& meta) const
    {
        return meta.AssetType == "SpriteAtlas";
    }

    bool SpriteAtlasEditor::OpenAsset(const AssetMeta& meta)
    {
        OpenSession(&meta);
        return m_Session.HasOpenAtlas();
    }

    void SpriteAtlasEditor::StashActiveSession()
    {
        if (!m_Session.HasOpenAtlas())
        {
            return;
        }
        m_SessionsByKey[m_Session.AssetPath] = m_Session;
    }

    bool SpriteAtlasEditor::ActivateStoredSession(const std::string& assetKey)
    {
        if (m_Session.HasOpenAtlas() && m_Session.AssetPath == assetKey)
        {
            return true;
        }

        StashActiveSession();
        const auto it = m_SessionsByKey.find(assetKey);
        if (it == m_SessionsByKey.end())
        {
            return false;
        }
        m_Session = it->second;
        return m_Session.HasOpenAtlas();
    }

    void SpriteAtlasEditor::DiscardStoredSession(const std::string& assetKey)
    {
        m_SessionsByKey.erase(assetKey);
        if (m_Session.AssetPath == assetKey)
        {
            m_Session.Clear();
        }
    }

    bool SpriteAtlasEditor::IsStoredSessionDirty(const std::string& assetKey) const
    {
        if (m_Session.AssetPath == assetKey)
        {
            return m_Session.Dirty;
        }
        const auto it = m_SessionsByKey.find(assetKey);
        return it != m_SessionsByKey.end() && it->second.Dirty;
    }

    void SpriteAtlasEditor::OpenSession(const AssetMeta* meta)
    {
        StashActiveSession();

        if (meta == nullptr)
        {
            m_Session.Clear();
            return;
        }

        if (ActivateStoredSession(meta->AssetPath))
        {
            return;
        }

        if (!AssetManager::HasInstance())
        {
            ME_LOG(LogEditor, Error, "SpriteAtlasEditor: AssetManager unavailable.");
            return;
        }

        std::shared_ptr<SpriteAtlas> atlas = AssetManager::Get().LoadAsset<SpriteAtlas>(meta->AssetPath);
        if (!atlas)
        {
            ME_LOG(LogEditor, Error, "SpriteAtlasEditor: failed to load '{}'.", meta->AssetPath);
            return;
        }

        atlas->RebindMemberOwnersAfterLoad();
        m_Session.AtlasAsset = atlas;
        m_Session.AssetPath = meta->AssetPath;
        m_Session.Dirty = false;
        m_Session.LastRebuildMessage.clear();
        m_SessionsByKey[m_Session.AssetPath] = m_Session;
    }

    bool SpriteAtlasEditor::SaveActiveAtlas()
    {
        if (!m_Session.HasOpenAtlas() || m_Context == nullptr)
        {
            return false;
        }

        SpriteEditorServices& services = m_Context->GetSpriteEditorServices();
        const GraphEditorResult atlasSave = services.SaveAtlas(*m_Session.AtlasAsset);
        if (!atlasSave.Ok)
        {
            ME_LOG(LogEditor, Error, "SpriteAtlasEditor: save atlas failed — {}", atlasSave.Message);
            return false;
        }

        // Cascade save member sprites (Baked refs after Rebuild).
        for (const std::shared_ptr<Sprite>& sprite : m_Session.AtlasAsset->GetMembers())
        {
            if (!sprite)
            {
                continue;
            }
            const GraphEditorResult spriteSave = services.SaveSprite(*sprite);
            if (!spriteSave.Ok)
            {
                ME_LOG(
                    LogEditor,
                    Warn,
                    "SpriteAtlasEditor: cascade save sprite '{}' failed — {}",
                    sprite->GetName(),
                    spriteSave.Message);
            }
        }

        m_Session.Dirty = false;
        m_SessionsByKey[m_Session.AssetPath] = m_Session;
        return true;
    }

    void SpriteAtlasEditor::NotifyChanged()
    {
        if (!m_Session.HasOpenAtlas())
        {
            return;
        }
        m_Session.Dirty = true;
        m_SessionsByKey[m_Session.AssetPath] = m_Session;
    }

    bool SpriteAtlasEditor::SubmitAddMembers(const std::vector<std::shared_ptr<Sprite>>& sprites)
    {
        if (!m_Session.HasOpenAtlas() || m_Context == nullptr)
        {
            return false;
        }

        EditorDocumentSession* documentSession = GetSpriteAtlasDocumentSession(*m_Context);
        if (documentSession == nullptr)
        {
            ME_LOG(LogEditor, Error, "SpriteAtlasEditor: no document session for command stack.");
            return false;
        }

        const GraphEditorResult result = m_Context->GetSpriteEditorServices().AddAtlasMembers(
            documentSession->GetCommandStack(),
            *m_Session.AtlasAsset,
            sprites);
        if (!result.Ok)
        {
            ME_LOG(LogEditor, Warn, "SpriteAtlasEditor: AddMembers failed — {}", result.Message);
            return false;
        }

        NotifyChanged();
        return true;
    }

    bool SpriteAtlasEditor::SubmitRemoveMember(const std::shared_ptr<Sprite>& sprite)
    {
        if (!m_Session.HasOpenAtlas() || m_Context == nullptr)
        {
            return false;
        }

        EditorDocumentSession* documentSession = GetSpriteAtlasDocumentSession(*m_Context);
        if (documentSession == nullptr)
        {
            return false;
        }

        const GraphEditorResult result = m_Context->GetSpriteEditorServices().RemoveAtlasMember(
            documentSession->GetCommandStack(),
            *m_Session.AtlasAsset,
            sprite);
        if (!result.Ok)
        {
            ME_LOG(LogEditor, Warn, "SpriteAtlasEditor: RemoveMember failed — {}", result.Message);
            return false;
        }

        NotifyChanged();
        return true;
    }

    bool SpriteAtlasEditor::SubmitRebuild()
    {
        if (!m_Session.HasOpenAtlas() || m_Context == nullptr)
        {
            return false;
        }

        if (!RenderSystem::HasInstance() || RenderSystem::Get().GetRHI() == nullptr)
        {
            m_Session.LastRebuildMessage = "Rebuild failed: RHI unavailable.";
            return false;
        }

        RHI& rhi = *RenderSystem::Get().GetRHI();
        const GraphEditorResult result =
            m_Context->GetSpriteEditorServices().RebuildAtlas(*m_Session.AtlasAsset, rhi);
        if (!result.Ok)
        {
            m_Session.LastRebuildMessage =
                result.Message.empty() ? "Rebuild failed." : result.Message;
            ME_LOG(LogEditor, Warn, "SpriteAtlasEditor: {}", m_Session.LastRebuildMessage);
            return false;
        }

        m_Session.LastRebuildMessage = "Rebuild succeeded.";
        NotifyChanged();
        return true;
    }

    void SpriteAtlasEditor::SetMaxSize(int32_t maxSize)
    {
        if (!m_Session.HasOpenAtlas())
        {
            return;
        }
        m_Session.AtlasAsset->SetMaxSize(maxSize);
        NotifyChanged();
    }

    void SpriteAtlasEditor::SetPadding(int32_t padding)
    {
        if (!m_Session.HasOpenAtlas())
        {
            return;
        }
        m_Session.AtlasAsset->SetPadding(padding);
        NotifyChanged();
    }
}
