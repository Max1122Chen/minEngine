#include "SpriteEditor.h"

#include "EditorGUIManager.h"
#include "Services/Sprite/SpriteEditorServices.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/EditorDockLayout.h"
#include "Shell/EditorInputHub.h"
#include "Shell/IEditorContext.h"
#include "UI/EditorWindows/SpriteEditorWindow.h"

#include "imgui.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

namespace minEngine
{
    SpriteEditor::SpriteEditor()
        : m_InspectorSource(*this)
    {
    }

    void SpriteEditor::Register(IEditorContext& context)
    {
        m_Context = &context;
        context.GetGUIManager().RegisterWindow(std::make_unique<SpriteEditorWindow>(context));
    }

    void SpriteEditor::Shutdown()
    {
        m_Session.Clear();
        m_SessionsByKey.clear();
        m_Context = nullptr;
    }

    void SpriteEditor::OnActivate(IEditorContext& context)
    {
        (void)context;
    }

    void SpriteEditor::OnDeactivate(IEditorContext& context)
    {
        (void)context;
        StashActiveSession();
    }

    void SpriteEditor::RegisterCommands(IEditorContext& context)
    {
        EditorCommandBinding saveCommand;
        saveCommand.Name = "Save Sprite";
        saveCommand.Chord = { ImGuiKey_S, true, false, false };
        saveCommand.CanExecute = [this]() { return m_Session.HasOpenSprite(); };
        saveCommand.Execute = [this]() { SaveActiveSprite(); };
        context.GetInputHub().RegisterActiveSubModuleCommand(std::move(saveCommand));
    }

    void SpriteEditor::UnregisterCommands(IEditorContext& context)
    {
        context.GetInputHub().ClearActiveSubModuleCommands();
    }

    void SpriteEditor::ApplyDefaultLayout(IEditorContext& context, ImGuiID dockspaceId)
    {
        (void)context;
        EditorDockLayout::BuildSpriteEditingLayout(dockspaceId);
    }

    bool SpriteEditor::CanOpenAsset(const AssetMeta& meta) const
    {
        return meta.AssetType == "Sprite";
    }

    bool SpriteEditor::OpenAsset(const AssetMeta& meta)
    {
        OpenSession(&meta);
        return m_Session.HasOpenSprite();
    }

    void SpriteEditor::StashActiveSession()
    {
        if (!m_Session.HasOpenSprite())
        {
            return;
        }
        m_SessionsByKey[m_Session.AssetPath] = m_Session;
    }

    bool SpriteEditor::ActivateStoredSession(const std::string& assetKey)
    {
        if (m_Session.HasOpenSprite() && m_Session.AssetPath == assetKey)
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
        return m_Session.HasOpenSprite();
    }

    void SpriteEditor::DiscardStoredSession(const std::string& assetKey)
    {
        m_SessionsByKey.erase(assetKey);
        if (m_Session.AssetPath == assetKey)
        {
            m_Session.Clear();
        }
    }

    bool SpriteEditor::IsStoredSessionDirty(const std::string& assetKey) const
    {
        if (m_Session.AssetPath == assetKey)
        {
            return m_Session.Dirty;
        }
        const auto it = m_SessionsByKey.find(assetKey);
        return it != m_SessionsByKey.end() && it->second.Dirty;
    }

    void SpriteEditor::OpenSession(const AssetMeta* meta)
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
            ME_LOG(LogEditor, Error, "SpriteEditor: AssetManager unavailable.");
            return;
        }

        std::shared_ptr<Sprite> sprite = ([&]() -> std::shared_ptr<Sprite> { std::string _loadAssetError; return std::dynamic_pointer_cast<Sprite>(AssetManager::Get().LoadAssetByPath(meta->AssetPath, _loadAssetError)); })();
        if (!sprite)
        {
            ME_LOG(LogEditor, Error, "SpriteEditor: failed to load '{}'.", meta->AssetPath);
            return;
        }

        m_Session.SpriteAsset = sprite;
        m_Session.AssetPath = meta->AssetPath;
        m_Session.Dirty = false;
        m_SessionsByKey[m_Session.AssetPath] = m_Session;
    }

    bool SpriteEditor::SaveActiveSprite()
    {
        if (!m_Session.HasOpenSprite() || m_Context == nullptr)
        {
            return false;
        }

        if (m_Session.SpriteAsset->GetSourceTexture() == nullptr)
        {
            ME_LOG(LogEditor, Error, "SpriteEditor: cannot save — SourceTexture is missing.");
            return false;
        }

        const GraphEditorResult result =
            m_Context->GetSpriteEditorServices().SaveSprite(*m_Session.SpriteAsset);
        if (!result.Ok)
        {
            ME_LOG(LogEditor, Error, "SpriteEditor: save failed — {}", result.Message);
            return false;
        }

        m_Session.Dirty = false;
        m_SessionsByKey[m_Session.AssetPath] = m_Session;
        return true;
    }

    void SpriteEditor::NotifyChanged()
    {
        if (!m_Session.HasOpenSprite())
        {
            return;
        }
        m_Session.Dirty = true;
        m_SessionsByKey[m_Session.AssetPath] = m_Session;
    }

    bool SpriteEditor::SubmitSetSourceRect(const Vector4& sourceRectPx)
    {
        if (!m_Session.HasOpenSprite() || m_Context == nullptr)
        {
            return false;
        }

        EditorDocumentSession* documentSession = GetSpriteDocumentSession(*m_Context);
        if (documentSession == nullptr)
        {
            ME_LOG(LogEditor, Error, "SpriteEditor: no document session for command stack.");
            return false;
        }

        const GraphEditorResult result = m_Context->GetSpriteEditorServices().SetSpriteSourceRect(
            documentSession->GetCommandStack(),
            *m_Session.SpriteAsset,
            sourceRectPx);
        if (!result.Ok)
        {
            ME_LOG(LogEditor, Warn, "SpriteEditor: SetSourceRect failed — {}", result.Message);
            return false;
        }

        NotifyChanged();
        return true;
    }
}
