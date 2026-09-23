#pragma once

#include "Core.h"
#include "SpriteAtlasEditorSession.h"
#include "SpriteAtlasInspectorSource.h"
#include "Shell/EditorSubModule.h"

#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class IEditorContext;
    class Sprite;

    /** SpriteAtlas document SubModule: members, pack settings, Rebuild preview via SpriteEditorServices. */
    class SpriteAtlasEditor : public EditorSubModule
    {
    public:
        static constexpr const char* kModuleId = "SpriteAtlas";

        SpriteAtlasEditor();

        std::string_view GetModuleId() const override { return kModuleId; }
        std::string_view GetDisplayName() const override { return "Sprite Atlas"; }

        void Register(IEditorContext& context) override;
        void Shutdown() override;

        void OnActivate(IEditorContext& context) override;
        void OnDeactivate(IEditorContext& context) override;
        void RegisterCommands(IEditorContext& context) override;
        void UnregisterCommands(IEditorContext& context) override;

        void Tick(float deltaTime) override { (void)deltaTime; }

        void ApplyDefaultLayout(IEditorContext& context, ImGuiID dockspaceId) override;

        IEditorInspectorSource* GetInspectorSource() override { return &m_InspectorSource; }
        const IEditorInspectorSource* GetInspectorSource() const override { return &m_InspectorSource; }

        bool CanOpenAsset(const AssetMeta& meta) const override;
        bool OpenAsset(const AssetMeta& meta) override;

        void OpenSession(const AssetMeta* meta);
        bool SaveActiveAtlas();
        void NotifyChanged();

        void StashActiveSession();
        bool ActivateStoredSession(const std::string& assetKey);
        void DiscardStoredSession(const std::string& assetKey);
        bool IsStoredSessionDirty(const std::string& assetKey) const;

        const SpriteAtlasEditorSession& GetSession() const { return m_Session; }
        SpriteAtlasEditorSession& GetSession() { return m_Session; }

        IEditorContext* GetEditorContext() const { return m_Context; }

        bool SubmitAddMembers(const std::vector<std::shared_ptr<Sprite>>& sprites);
        bool SubmitRemoveMember(const std::shared_ptr<Sprite>& sprite);
        bool SubmitRebuild();
        void SetMaxSize(int32_t maxSize);
        void SetPadding(int32_t padding);
        void SetPaddingMode(SpriteAtlasPaddingMode mode);

    private:
        SpriteAtlasInspectorSource m_InspectorSource;
        IEditorContext* m_Context = nullptr;
        SpriteAtlasEditorSession m_Session;
        std::unordered_map<std::string, SpriteAtlasEditorSession> m_SessionsByKey;
    };
}
