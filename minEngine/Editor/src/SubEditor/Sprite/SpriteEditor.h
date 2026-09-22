#pragma once

#include "Core.h"
#include "SpriteEditorSession.h"
#include "SpriteInspectorSource.h"
#include "Shell/EditorSubModule.h"

#include "Runtime/Core/Math/Math.h"
#include "Runtime/Resource/AssetMeta.h"

#include <string>
#include <unordered_map>

namespace minEngine
{
    class IEditorContext;

    /** Sprite document SubModule: Source texture preview + SourceRect editing via SpriteEditorServices. */
    class SpriteEditor : public EditorSubModule
    {
    public:
        static constexpr const char* kModuleId = "Sprite";

        SpriteEditor();

        std::string_view GetModuleId() const override { return kModuleId; }
        std::string_view GetDisplayName() const override { return "Sprite"; }

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
        bool SaveActiveSprite();
        void NotifyChanged();

        void StashActiveSession();
        bool ActivateStoredSession(const std::string& assetKey);
        void DiscardStoredSession(const std::string& assetKey);
        bool IsStoredSessionDirty(const std::string& assetKey) const;

        const SpriteEditorSession& GetSession() const { return m_Session; }
        SpriteEditorSession& GetSession() { return m_Session; }

        IEditorContext* GetEditorContext() const { return m_Context; }

        bool SubmitSetSourceRect(const Vector4& sourceRectPx);

    private:
        SpriteInspectorSource m_InspectorSource;
        IEditorContext* m_Context = nullptr;
        SpriteEditorSession m_Session;
        std::unordered_map<std::string, SpriteEditorSession> m_SessionsByKey;
    };
}
