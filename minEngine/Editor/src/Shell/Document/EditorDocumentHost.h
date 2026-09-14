#pragma once

#include "Shell/Document/EditorDocumentId.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/Document/EditorDocumentTypeRegistry.h"
#include "UI/Dialogs/EditorUnsavedChangesDialog.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class AssetMeta;
    class IEditorContext;

    class EditorDocumentHost
    {
    public:
        EditorDocumentTypeRegistry& GetTypeRegistry() { return m_TypeRegistry; }
        const EditorDocumentTypeRegistry& GetTypeRegistry() const { return m_TypeRegistry; }

        void SetContext(IEditorContext* context) { m_Context = context; }

        const std::vector<std::unique_ptr<EditorDocumentSession>>& GetSessions() const { return m_Sessions; }
        void ClearAllCommandStacks();

        EditorDocumentSession* GetActiveSession();
        const EditorDocumentSession* GetActiveSession() const;
        EditorDocumentSession* FindSession(EditorDocumentId id);
        const EditorDocumentSession* FindSession(EditorDocumentId id) const;
        EditorDocumentSession* FindSessionByAssetKey(std::string_view assetKey);
        EditorDocumentSession* FindFirstSessionOfType(std::string_view typeId);
        size_t GetSessionIndex(EditorDocumentId id) const;

        bool Activate(EditorDocumentId id);
        bool OpenOrFocus(const AssetMeta& meta);
        bool RequestClose(EditorDocumentId id);
        bool RequestCloseOthers(EditorDocumentId keepId);
        bool RequestCloseToTheRight(EditorDocumentId fromId);
        bool RequestCloseSaved();
        void Reorder(size_t fromIndex, size_t toIndex);

        void SyncDirtyFlags();
        void ReserveTabBarWorkArea();
        void DrawTabBar();
        void DrawPendingDialogs();

        /** Create a session for an already-open asset without re-binding/reloading. */
        bool AdoptOpenDocument(std::string typeId, std::string assetKey, std::string title);

        EditorCommandStack* GetActiveCommandStack();

    private:
        enum class PendingCloseKind
        {
            None,
            Single,
            Batch,
        };

        EditorDocumentId AllocateId();
        bool ActivateInternal(EditorDocumentId id, bool resetLayoutIfTypeChanged);
        bool CloseSessionInternal(EditorDocumentId id);
        void CollectCloseable(
            const std::function<bool(const EditorDocumentSession&)>& predicate,
            std::vector<EditorDocumentId>& outIds) const;
        void BeginBatchClose(std::vector<EditorDocumentId> ids);
        void ContinuePendingClose();
        void HandleUnsavedChoice(UnsavedChangesChoice choice);
        float GetTabBarHeight() const;

        IEditorContext* m_Context = nullptr;
        EditorDocumentTypeRegistry m_TypeRegistry;
        std::vector<std::unique_ptr<EditorDocumentSession>> m_Sessions;
        EditorDocumentId m_ActiveId{};
        uint64_t m_NextId = 1;

        PendingCloseKind m_PendingKind = PendingCloseKind::None;
        std::vector<EditorDocumentId> m_PendingCloseIds;
        size_t m_PendingCloseIndex = 0;
        EditorUnsavedChangesDialog m_UnsavedDialog;
        std::string m_PendingMessage;
        EditorDocumentId m_PendingSelectId{};
        /** While true, ImGui tab selection may lag ActivateInternal; do not sync UI→Host (avoids steal). */
        bool m_WaitingForTabUiSync = false;
    };
}
