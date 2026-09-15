#include "Shell/Document/EditorDocumentHost.h"

#include "Shell/EditorInputHub.h"
#include "Shell/IEditorContext.h"
#include "Services/AssetWorkflowModule.h"
#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorAssetTypeIcons.h"
#include "Runtime/Resource/AssetMeta.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Core/Log/LogSystem.h"

#include "imgui.h"

#include <algorithm>
#include <filesystem>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace minEngine
{
    EditorDocumentSession* EditorDocumentHost::GetActiveSession()
    {
        return FindSession(m_ActiveId);
    }

    const EditorDocumentSession* EditorDocumentHost::GetActiveSession() const
    {
        return FindSession(m_ActiveId);
    }

    EditorDocumentSession* EditorDocumentHost::FindSession(EditorDocumentId id)
    {
        for (std::unique_ptr<EditorDocumentSession>& session : m_Sessions)
        {
            if (session && session->GetId() == id)
            {
                return session.get();
            }
        }
        return nullptr;
    }

    const EditorDocumentSession* EditorDocumentHost::FindSession(EditorDocumentId id) const
    {
        for (const std::unique_ptr<EditorDocumentSession>& session : m_Sessions)
        {
            if (session && session->GetId() == id)
            {
                return session.get();
            }
        }
        return nullptr;
    }

    EditorDocumentSession* EditorDocumentHost::FindSessionByAssetKey(std::string_view assetKey)
    {
        for (std::unique_ptr<EditorDocumentSession>& session : m_Sessions)
        {
            if (session && session->GetAssetKey() == assetKey)
            {
                return session.get();
            }
        }
        return nullptr;
    }

    EditorDocumentSession* EditorDocumentHost::FindFirstSessionOfType(std::string_view typeId)
    {
        for (std::unique_ptr<EditorDocumentSession>& session : m_Sessions)
        {
            if (session && session->GetTypeId() == typeId)
            {
                return session.get();
            }
        }
        return nullptr;
    }

    size_t EditorDocumentHost::GetSessionIndex(EditorDocumentId id) const
    {
        for (size_t i = 0; i < m_Sessions.size(); ++i)
        {
            if (m_Sessions[i] && m_Sessions[i]->GetId() == id)
            {
                return i;
            }
        }
        return static_cast<size_t>(-1);
    }

    EditorDocumentId EditorDocumentHost::AllocateId()
    {
        EditorDocumentId id;
        id.Value = m_NextId++;
        return id;
    }

    EditorCommandStack* EditorDocumentHost::GetActiveCommandStack()
    {
        EditorDocumentSession* session = GetActiveSession();
        return session != nullptr ? &session->GetCommandStack() : nullptr;
    }

    void EditorDocumentHost::ClearAllCommandStacks()
    {
        for (std::unique_ptr<EditorDocumentSession>& session : m_Sessions)
        {
            if (session)
            {
                session->GetCommandStack().Clear();
            }
        }
    }

    void EditorDocumentHost::SyncDirtyFlags()
    {
        if (m_Context == nullptr)
        {
            return;
        }

        for (std::unique_ptr<EditorDocumentSession>& session : m_Sessions)
        {
            if (!session)
            {
                continue;
            }
            const EditorDocumentTypeInfo* typeInfo = m_TypeRegistry.Find(session->GetTypeId());
            if (typeInfo != nullptr && typeInfo->QueryDirty)
            {
                session->SetDirty(typeInfo->QueryDirty(*m_Context, *session));
            }
        }
    }

    bool EditorDocumentHost::Activate(EditorDocumentId id)
    {
        return ActivateInternal(id, true);
    }

    bool EditorDocumentHost::ActivateInternal(EditorDocumentId id, bool resetLayoutIfTypeChanged)
    {
        if (m_Context == nullptr)
        {
            return false;
        }

        if (m_ActiveId == id)
        {
            return true;
        }

        EditorDocumentSession* session = FindSession(id);
        if (session == nullptr)
        {
            return false;
        }

        const EditorDocumentTypeInfo* typeInfo = m_TypeRegistry.Find(session->GetTypeId());
        if (typeInfo == nullptr)
        {
            return false;
        }

        const std::string previousType =
            GetActiveSession() != nullptr ? GetActiveSession()->GetTypeId() : std::string{};

        if (typeInfo->ActivateSession && !typeInfo->ActivateSession(*m_Context, *session))
        {
            return false;
        }

        const bool typeChanged = previousType != session->GetTypeId();
        if (!typeInfo->ModuleId.empty())
        {
            // Always bring the owning sub-editor to front when activating a document.
            if (!m_Context->ActivateSubModule(typeInfo->ModuleId, resetLayoutIfTypeChanged && typeChanged))
            {
                ME_LOG(
                    LogEditor,
                    Warn,
                    "DocumentHost: ActivateSubModule('{}') failed for session '{}'.",
                    typeInfo->ModuleId,
                    session->GetTitle());
                return false;
            }
        }

        m_ActiveId = id;
        m_PendingSelectId = id;
        m_WaitingForTabUiSync = true;
        return true;
    }

    bool EditorDocumentHost::OpenOrFocus(const AssetMeta& meta)
    {
        if (m_Context == nullptr)
        {
            return false;
        }

        const EditorDocumentTypeInfo* typeInfo = m_TypeRegistry.FindForAsset(meta);
        if (typeInfo == nullptr)
        {
            return false;
        }

        const std::string assetKey =
            typeInfo->MakeAssetKey ? typeInfo->MakeAssetKey(meta) : meta.AssetPath;
        if (assetKey.empty())
        {
            return false;
        }

        if (EditorDocumentSession* existing = FindSessionByAssetKey(assetKey))
        {
            return Activate(existing->GetId());
        }

        if (!typeInfo->AllowMultipleSessions)
        {
            std::vector<EditorDocumentId> sameTypeIds;
            for (const std::unique_ptr<EditorDocumentSession>& session : m_Sessions)
            {
                if (session && session->GetTypeId() == typeInfo->TypeId)
                {
                    sameTypeIds.push_back(session->GetId());
                }
            }
            SyncDirtyFlags();
            for (EditorDocumentId id : sameTypeIds)
            {
                EditorDocumentSession* existingSameType = FindSession(id);
                if (existingSameType != nullptr && existingSameType->IsDirty())
                {
                    return false;
                }
                CloseSessionInternal(id);
            }
        }

        const std::string title =
            typeInfo->MakeTitle ? typeInfo->MakeTitle(meta) : std::filesystem::path(assetKey).filename().string();

        auto session = std::make_unique<EditorDocumentSession>(AllocateId(), typeInfo->TypeId, assetKey, title);
        EditorDocumentSession* raw = session.get();
        if (typeInfo->BindSession && !typeInfo->BindSession(*m_Context, meta, *raw))
        {
            return false;
        }

        m_Sessions.push_back(std::move(session));
        return ActivateInternal(raw->GetId(), true);
    }

    bool EditorDocumentHost::CloseSessionInternal(EditorDocumentId id)
    {
        if (m_Context == nullptr)
        {
            return false;
        }

        EditorDocumentSession* session = FindSession(id);
        if (session == nullptr)
        {
            return false;
        }

        const EditorDocumentTypeInfo* typeInfo = m_TypeRegistry.Find(session->GetTypeId());
        if (typeInfo != nullptr)
        {
            size_t typeCount = 0;
            for (const std::unique_ptr<EditorDocumentSession>& other : m_Sessions)
            {
                if (other && other->GetTypeId() == session->GetTypeId())
                {
                    ++typeCount;
                }
            }
            if (!typeInfo->AllowCloseLastOfType && typeCount <= 1)
            {
                ME_LOG(LogEditor, Info, "DocumentHost: refusing to close last session of type '{}'.", typeInfo->TypeId);
                return false;
            }

            if (typeInfo->CloseSession)
            {
                typeInfo->CloseSession(*m_Context, *session);
            }
        }

        const size_t index = GetSessionIndex(id);
        const bool wasActive = m_ActiveId == id;

        ClosedDocumentRecord closedRecord;
        closedRecord.TypeId = session->GetTypeId();
        closedRecord.AssetKey = session->GetAssetKey();
        closedRecord.Title = session->GetTitle();

        if (index < m_Sessions.size())
        {
            m_Sessions.erase(m_Sessions.begin() + static_cast<std::ptrdiff_t>(index));
        }

        if (!closedRecord.AssetKey.empty())
        {
            PushClosedHistory(std::move(closedRecord));
        }

        if (wasActive)
        {
            m_ActiveId = {};
            if (!m_Sessions.empty())
            {
                const size_t nextIndex = std::min(index, m_Sessions.size() - 1);
                ActivateInternal(m_Sessions[nextIndex]->GetId(), true);
            }
        }
        return true;
    }

    bool EditorDocumentHost::RequestClose(EditorDocumentId id)
    {
        EditorDocumentSession* session = FindSession(id);
        if (session == nullptr)
        {
            return true;
        }

        SyncDirtyFlags();
        if (!session->IsDirty())
        {
            return CloseSessionInternal(id);
        }

        if (m_PendingKind != PendingCloseKind::None)
        {
            return false;
        }

        m_PendingKind = PendingCloseKind::Single;
        m_PendingCloseIds = {id};
        m_PendingCloseIndex = 0;
        m_PendingMessage = "Save changes to \"" + session->GetTitle() + "\" before closing?";
        m_UnsavedDialog.Open(m_PendingMessage.c_str());
        return false;
    }

    void EditorDocumentHost::CollectCloseable(
        const std::function<bool(const EditorDocumentSession&)>& predicate,
        std::vector<EditorDocumentId>& outIds) const
    {
        outIds.clear();
        for (const std::unique_ptr<EditorDocumentSession>& session : m_Sessions)
        {
            if (!session || session->IsPinned())
            {
                continue;
            }
            if (predicate(*session))
            {
                outIds.push_back(session->GetId());
            }
        }
    }

    void EditorDocumentHost::BeginBatchClose(std::vector<EditorDocumentId> ids)
    {
        if (ids.empty() || m_PendingKind != PendingCloseKind::None)
        {
            return;
        }

        SyncDirtyFlags();
        m_PendingKind = PendingCloseKind::Batch;
        m_PendingCloseIds = std::move(ids);
        m_PendingCloseIndex = 0;
        ContinuePendingClose();
    }

    void EditorDocumentHost::ContinuePendingClose()
    {
        while (m_PendingCloseIndex < m_PendingCloseIds.size())
        {
            const EditorDocumentId id = m_PendingCloseIds[m_PendingCloseIndex];
            EditorDocumentSession* session = FindSession(id);
            if (session == nullptr)
            {
                ++m_PendingCloseIndex;
                continue;
            }

            SyncDirtyFlags();
            if (!session->IsDirty())
            {
                CloseSessionInternal(id);
                ++m_PendingCloseIndex;
                continue;
            }

            m_PendingMessage = "Save changes to \"" + session->GetTitle() + "\" before closing?";
            if (m_PendingKind == PendingCloseKind::Batch && m_PendingCloseIds.size() > 1)
            {
                m_PendingMessage += "\n\n(Remaining dirty tabs will ask next, or use Cancel to abort.)";
            }
            m_UnsavedDialog.Open(m_PendingMessage.c_str());
            return;
        }

        m_PendingKind = PendingCloseKind::None;
        m_PendingCloseIds.clear();
        m_PendingCloseIndex = 0;
    }

    void EditorDocumentHost::HandleUnsavedChoice(UnsavedChangesChoice choice)
    {
        if (m_PendingKind == PendingCloseKind::None || m_PendingCloseIndex >= m_PendingCloseIds.size())
        {
            m_PendingKind = PendingCloseKind::None;
            return;
        }

        const EditorDocumentId id = m_PendingCloseIds[m_PendingCloseIndex];
        EditorDocumentSession* session = FindSession(id);

        if (choice == UnsavedChangesChoice::Cancel || choice == UnsavedChangesChoice::None)
        {
            m_PendingKind = PendingCloseKind::None;
            m_PendingCloseIds.clear();
            m_PendingCloseIndex = 0;
            return;
        }

        if (choice == UnsavedChangesChoice::Save && session != nullptr && m_Context != nullptr)
        {
            const EditorDocumentTypeInfo* typeInfo = m_TypeRegistry.Find(session->GetTypeId());
            if (typeInfo == nullptr || !typeInfo->SaveSession || !typeInfo->SaveSession(*m_Context, *session))
            {
                ME_LOG(LogEditor, Warn, "DocumentHost: save failed; close cancelled.");
                m_PendingKind = PendingCloseKind::None;
                m_PendingCloseIds.clear();
                m_PendingCloseIndex = 0;
                return;
            }
            session->SetDirty(false);
        }

        CloseSessionInternal(id);
        ++m_PendingCloseIndex;
        ContinuePendingClose();
    }

    bool EditorDocumentHost::RequestCloseOthers(EditorDocumentId keepId)
    {
        std::vector<EditorDocumentId> ids;
        CollectCloseable(
            [keepId](const EditorDocumentSession& session)
            {
                return session.GetId() != keepId;
            },
            ids);
        BeginBatchClose(std::move(ids));
        return m_PendingKind == PendingCloseKind::None;
    }

    bool EditorDocumentHost::RequestCloseToTheRight(EditorDocumentId fromId)
    {
        const size_t fromIndex = GetSessionIndex(fromId);
        if (fromIndex >= m_Sessions.size())
        {
            return true;
        }

        std::vector<EditorDocumentId> ids;
        for (size_t i = fromIndex + 1; i < m_Sessions.size(); ++i)
        {
            if (m_Sessions[i] && !m_Sessions[i]->IsPinned())
            {
                ids.push_back(m_Sessions[i]->GetId());
            }
        }
        BeginBatchClose(std::move(ids));
        return m_PendingKind == PendingCloseKind::None;
    }

    bool EditorDocumentHost::RequestCloseSaved()
    {
        SyncDirtyFlags();
        std::vector<EditorDocumentId> ids;
        CollectCloseable(
            [](const EditorDocumentSession& session)
            {
                return !session.IsDirty();
            },
            ids);
        BeginBatchClose(std::move(ids));
        return m_PendingKind == PendingCloseKind::None;
    }

    bool EditorDocumentHost::RequestCloseAll()
    {
        std::vector<EditorDocumentId> ids;
        CollectCloseable(
            [](const EditorDocumentSession&)
            {
                return true;
            },
            ids);
        BeginBatchClose(std::move(ids));
        return m_PendingKind == PendingCloseKind::None;
    }

    void EditorDocumentHost::PushClosedHistory(ClosedDocumentRecord record)
    {
        m_ClosedHistory.erase(
            std::remove_if(
                m_ClosedHistory.begin(),
                m_ClosedHistory.end(),
                [&](const ClosedDocumentRecord& entry)
                {
                    return entry.AssetKey == record.AssetKey;
                }),
            m_ClosedHistory.end());
        m_ClosedHistory.push_back(std::move(record));
        while (m_ClosedHistory.size() > kClosedHistoryLimit)
        {
            m_ClosedHistory.erase(m_ClosedHistory.begin());
        }
    }

    void EditorDocumentHost::MoveSessionToPinnedZone(EditorDocumentId id, bool pinned)
    {
        const size_t fromIndex = GetSessionIndex(id);
        if (fromIndex >= m_Sessions.size() || !m_Sessions[fromIndex])
        {
            return;
        }

        std::unique_ptr<EditorDocumentSession> moved = std::move(m_Sessions[fromIndex]);
        m_Sessions.erase(m_Sessions.begin() + static_cast<std::ptrdiff_t>(fromIndex));

        size_t insertIndex = 0;
        if (pinned)
        {
            while (insertIndex < m_Sessions.size() && m_Sessions[insertIndex]
                   && m_Sessions[insertIndex]->IsPinned())
            {
                ++insertIndex;
            }
        }
        else
        {
            insertIndex = 0;
            while (insertIndex < m_Sessions.size() && m_Sessions[insertIndex]
                   && m_Sessions[insertIndex]->IsPinned())
            {
                ++insertIndex;
            }
        }

        m_Sessions.insert(m_Sessions.begin() + static_cast<std::ptrdiff_t>(insertIndex), std::move(moved));
    }

    void EditorDocumentHost::SetSessionPinned(EditorDocumentId id, bool pinned)
    {
        EditorDocumentSession* session = FindSession(id);
        if (session == nullptr || session->IsPinned() == pinned)
        {
            return;
        }

        session->SetPinned(pinned);
        MoveSessionToPinnedZone(id, pinned);
    }

    bool EditorDocumentHost::ReopenClosedTab()
    {
        while (!m_ClosedHistory.empty())
        {
            const ClosedDocumentRecord record = std::move(m_ClosedHistory.back());
            m_ClosedHistory.pop_back();
            if (record.AssetKey.empty())
            {
                continue;
            }

            if (FindSessionByAssetKey(record.AssetKey) != nullptr)
            {
                continue;
            }

            if (!AssetManager::HasInstance())
            {
                return false;
            }

            const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(record.AssetKey);
            if (meta == nullptr)
            {
                ME_LOG(
                    LogEditor,
                    Warn,
                    "DocumentHost: reopen skipped; asset meta missing '{}'.",
                    record.AssetKey);
                continue;
            }

            return OpenOrFocus(*meta);
        }

        return false;
    }

    bool EditorDocumentHost::ActivateAdjacentTab(int delta)
    {
        if (m_Sessions.empty() || delta == 0)
        {
            return false;
        }

        size_t index = GetSessionIndex(m_ActiveId);
        if (index >= m_Sessions.size())
        {
            index = 0;
        }

        const int count = static_cast<int>(m_Sessions.size());
        int nextIndex = (static_cast<int>(index) + delta) % count;
        if (nextIndex < 0)
        {
            nextIndex += count;
        }

        if (!m_Sessions[static_cast<size_t>(nextIndex)])
        {
            return false;
        }

        return Activate(m_Sessions[static_cast<size_t>(nextIndex)]->GetId());
    }

    bool EditorDocumentHost::CopyAssetPath(EditorDocumentId id, bool relativeToProjectRoot) const
    {
        const EditorDocumentSession* session = FindSession(id);
        if (session == nullptr || session->GetAssetKey().empty())
        {
            return false;
        }

        if (relativeToProjectRoot)
        {
            ImGui::SetClipboardText(session->GetAssetKey().c_str());
            return true;
        }

        if (!AssetManager::HasInstance())
        {
            ImGui::SetClipboardText(session->GetAssetKey().c_str());
            return true;
        }

        const std::filesystem::path absolutePath =
            AssetManager::Get().ResolveAssetAbsolutePath(session->GetAssetKey());
        ImGui::SetClipboardText(absolutePath.string().c_str());
        return true;
    }

    bool EditorDocumentHost::RevealInOsExplorer(EditorDocumentId id) const
    {
        const EditorDocumentSession* session = FindSession(id);
        if (session == nullptr || session->GetAssetKey().empty() || !AssetManager::HasInstance())
        {
            return false;
        }

        std::filesystem::path absolutePath =
            AssetManager::Get().ResolveAssetAbsolutePath(session->GetAssetKey());
        absolutePath = absolutePath.lexically_normal().make_preferred();

        std::error_code error;
        if (!std::filesystem::exists(absolutePath, error))
        {
            ME_LOG(
                LogEditor,
                Warn,
                "DocumentHost: reveal failed; missing '{}' (assetKey='{}').",
                absolutePath.string(),
                session->GetAssetKey());
            return false;
        }

#if defined(_WIN32)
        const std::wstring widePath = absolutePath.wstring();
        PIDLIST_ABSOLUTE itemIdList = ILCreateFromPathW(widePath.c_str());
        if (itemIdList == nullptr)
        {
            ME_LOG(
                LogEditor,
                Warn,
                "DocumentHost: ILCreateFromPathW failed for '{}'.",
                absolutePath.string());
            return false;
        }

        const HRESULT openResult = SHOpenFolderAndSelectItems(itemIdList, 0, nullptr, 0);
        ILFree(itemIdList);
        if (FAILED(openResult))
        {
            ME_LOG(
                LogEditor,
                Warn,
                "DocumentHost: SHOpenFolderAndSelectItems failed (hr=0x{:08X}) for '{}'.",
                static_cast<unsigned>(openResult),
                absolutePath.string());
            return false;
        }

        return true;
#else
        ME_LOG(LogEditor, Warn, "DocumentHost: Reveal in Explorer is only implemented on Windows.");
        return false;
#endif
    }

    std::string EditorDocumentHost::MakeTabLabel(const EditorDocumentSession& session) const
    {
        // Leading spaces reserve room for the FA glyph drawn over the tab item.
        std::string label = "    ";
        if (session.IsPinned())
        {
            label += "* ";
        }

        label += session.GetTitle();
        // Unique ImGui id suffix so titles can collide across types.
        label += "###doc";
        label += std::to_string(session.GetId().Value);
        return label;
    }

    void EditorDocumentHost::DrawTabTypeIcon(const EditorDocumentSession& session) const
    {
        if (m_Context == nullptr)
        {
            return;
        }

        const EditorAppearance& appearance = m_Context->GetEditorAppearance();
        ImFont* iconFont =
            EditorAssetTypeIcons::ResolveFontForDocumentTypeId(appearance, session.GetTypeId());
        const char* glyph = EditorAssetTypeIcons::GlyphForDocumentTypeId(session.GetTypeId());
        if (iconFont == nullptr || glyph == nullptr || glyph[0] == '\0')
        {
            return;
        }

        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        constexpr float kIconFontSize = 13.0f;
        ImGui::PushFont(iconFont, kIconFontSize);
        const ImVec2 glyphSize = ImGui::CalcTextSize(glyph);
        const float glyphX = itemMin.x + 6.0f;
        const float glyphY = itemMin.y + (itemMax.y - itemMin.y - glyphSize.y) * 0.5f;
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(glyphX, glyphY),
            ImGui::GetColorU32(ImGuiCol_Text),
            glyph);
        ImGui::PopFont();
    }

    void EditorDocumentHost::RegisterInputCommands(EditorInputHub& inputHub)
    {
        {
            EditorCommandBinding reopenCommand;
            reopenCommand.Name = "Reopen Closed Tab";
            reopenCommand.Chord = {ImGuiKey_T, true, true, false};
            reopenCommand.CanExecute = [this]() { return CanReopenClosedTab(); };
            reopenCommand.Execute = [this]() { ReopenClosedTab(); };
            inputHub.RegisterGlobalCommand(std::move(reopenCommand));
        }
        {
            EditorCommandBinding nextTabCommand;
            nextTabCommand.Name = "Next Document Tab";
            nextTabCommand.Chord = {ImGuiKey_Tab, true, false, false};
            nextTabCommand.CanExecute = [this]() { return !m_Sessions.empty(); };
            nextTabCommand.Execute = [this]() { ActivateAdjacentTab(1); };
            inputHub.RegisterGlobalCommand(std::move(nextTabCommand));
        }
        {
            EditorCommandBinding prevTabCommand;
            prevTabCommand.Name = "Previous Document Tab";
            prevTabCommand.Chord = {ImGuiKey_Tab, true, true, false};
            prevTabCommand.CanExecute = [this]() { return !m_Sessions.empty(); };
            prevTabCommand.Execute = [this]() { ActivateAdjacentTab(-1); };
            inputHub.RegisterGlobalCommand(std::move(prevTabCommand));
        }
    }

    void EditorDocumentHost::Reorder(size_t fromIndex, size_t toIndex)
    {
        if (fromIndex >= m_Sessions.size() || toIndex >= m_Sessions.size() || fromIndex == toIndex)
        {
            return;
        }
        if (!m_Sessions[fromIndex] || !m_Sessions[toIndex])
        {
            return;
        }
        // Keep pinned and unpinned zones separate (explicit Pin menu; drag does not cross).
        if (m_Sessions[fromIndex]->IsPinned() != m_Sessions[toIndex]->IsPinned())
        {
            return;
        }

        std::unique_ptr<EditorDocumentSession> moved = std::move(m_Sessions[fromIndex]);
        m_Sessions.erase(m_Sessions.begin() + static_cast<std::ptrdiff_t>(fromIndex));
        m_Sessions.insert(m_Sessions.begin() + static_cast<std::ptrdiff_t>(toIndex), std::move(moved));
    }

    void EditorDocumentHost::DrawPendingDialogs()
    {
        if (!m_UnsavedDialog.IsOpen())
        {
            return;
        }

        const UnsavedChangesChoice choice = m_UnsavedDialog.Draw();
        if (choice != UnsavedChangesChoice::None)
        {
            HandleUnsavedChoice(choice);
        }
    }

    float EditorDocumentHost::GetTabBarHeight() const
    {
        return ImGui::GetFrameHeightWithSpacing() + 8.0f;
    }

    void EditorDocumentHost::ReserveTabBarWorkArea()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (viewport == nullptr)
        {
            return;
        }

        // Always reserve so mid-frame OpenOrFocus does not jump DockSpace layout.
        const float tabBarHeight = GetTabBarHeight();
        viewport->WorkPos.y += tabBarHeight;
        viewport->WorkSize.y = std::max(0.0f, viewport->WorkSize.y - tabBarHeight);
    }

    bool EditorDocumentHost::AdoptOpenDocument(std::string typeId, std::string assetKey, std::string title)
    {
        if (m_Context == nullptr || typeId.empty() || assetKey.empty())
        {
            return false;
        }

        if (EditorDocumentSession* existing = FindSessionByAssetKey(assetKey))
        {
            return ActivateInternal(existing->GetId(), false);
        }

        if (m_TypeRegistry.Find(typeId) == nullptr)
        {
            return false;
        }

        auto session =
            std::make_unique<EditorDocumentSession>(AllocateId(), std::move(typeId), assetKey, std::move(title));
        const EditorDocumentId id = session->GetId();
        m_Sessions.push_back(std::move(session));
        // No BindSession — caller already has the asset open (e.g. startup scene).
        return ActivateInternal(id, false);
    }

    void EditorDocumentHost::DrawTabBar()
    {
        SyncDirtyFlags();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (viewport == nullptr)
        {
            return;
        }

        // Work area was already shrunk in ReserveTabBarWorkArea(); place strip above it.
        const float tabBarHeight = GetTabBarHeight();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y - tabBarHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, tabBarHeight));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));

        constexpr ImGuiWindowFlags kTabHostFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;

        const bool began = ImGui::Begin("##EditorDocumentTabBar", nullptr, kTabHostFlags);
        if (began && !m_Sessions.empty())
        {
            if (ImGui::BeginTabBar(
                    "##DocumentTabs",
                    ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll
                        | ImGuiTabBarFlags_AutoSelectNewTabs))
            {
                EditorDocumentId uiSelectedId{};

                for (size_t i = 0; i < m_Sessions.size(); ++i)
                {
                    EditorDocumentSession* session = m_Sessions[i].get();
                    if (session == nullptr)
                    {
                        continue;
                    }

                    bool open = true;
                    ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
                    if (session->IsDirty())
                    {
                        flags |= ImGuiTabItemFlags_UnsavedDocument;
                    }
                    if (session->GetId() == m_PendingSelectId)
                    {
                        flags |= ImGuiTabItemFlags_SetSelected;
                        m_PendingSelectId = {};
                    }

                    ImGui::PushID(static_cast<int>(session->GetId().Value));
                    const std::string tabLabel = MakeTabLabel(*session);
                    const bool selected = ImGui::BeginTabItem(tabLabel.c_str(), &open, flags);

                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    {
                        const ImVec2 mouse = ImGui::GetMousePos();
                        const ImVec2 itemSize = ImGui::GetItemRectSize();
                        ImDrawList* drawList = ImGui::GetForegroundDrawList();
                        const ImVec2 ghostMin(mouse.x - itemSize.x * 0.5f, mouse.y - itemSize.y * 0.5f);
                        const ImVec2 ghostMax(ghostMin.x + itemSize.x, ghostMin.y + itemSize.y);
                        const ImU32 fill = ImGui::GetColorU32(ImGuiCol_HeaderHovered, 0.55f);
                        const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border, 0.9f);
                        const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text, 0.95f);
                        drawList->AddRectFilled(ghostMin, ghostMax, fill, 4.0f);
                        drawList->AddRect(ghostMin, ghostMax, border, 4.0f);
                        const ImVec2 textSize = ImGui::CalcTextSize(session->GetTitle().c_str());
                        drawList->AddText(
                            ImVec2(
                                ghostMin.x + (itemSize.x - textSize.x) * 0.5f,
                                ghostMin.y + (itemSize.y - textSize.y) * 0.5f),
                            textColor,
                            session->GetTitle().c_str());
                    }

                    if (ImGui::BeginPopupContextItem("DocumentTabContext"))
                    {
                        DrawTabContextMenu(*session, i);
                        ImGui::EndPopup();
                    }

                    DrawTabTypeIcon(*session);

                    if (selected)
                    {
                        uiSelectedId = session->GetId();
                        ImGui::EndTabItem();
                    }

                    // BeginTabItem can return true (selected) in the same frame the user clicks X
                    // (open→false). Always honor close — Pin only protects batch Close Others/All.
                    if (!open)
                    {
                        RequestClose(session->GetId());
                    }

                    ImGui::PopID();
                }

                // Sync ImGui tab selection → Host. Wait until UI catches programmatic Activate
                // so a stale previously-selected tab cannot steal focus in the same frame.
                if (uiSelectedId.IsValid())
                {
                    if (m_WaitingForTabUiSync)
                    {
                        if (uiSelectedId == m_ActiveId)
                        {
                            m_WaitingForTabUiSync = false;
                        }
                    }
                    else if (uiSelectedId != m_ActiveId)
                    {
                        ActivateInternal(uiSelectedId, true);
                    }
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
    }

    void EditorDocumentHost::DrawTabContextMenu(EditorDocumentSession& session, size_t sessionIndex)
    {
        if (ImGui::MenuItem("Close"))
        {
            RequestClose(session.GetId());
        }
        if (ImGui::MenuItem("Close Others", nullptr, false, m_Sessions.size() > 1))
        {
            RequestCloseOthers(session.GetId());
        }
        if (ImGui::MenuItem("Close to the Right", nullptr, false, sessionIndex + 1 < m_Sessions.size()))
        {
            RequestCloseToTheRight(session.GetId());
        }
        if (ImGui::MenuItem("Close Saved"))
        {
            RequestCloseSaved();
        }
        if (ImGui::MenuItem("Close All", nullptr, false, !m_Sessions.empty()))
        {
            RequestCloseAll();
        }

        ImGui::Separator();
        if (session.IsPinned())
        {
            if (ImGui::MenuItem("Unpin Tab"))
            {
                SetSessionPinned(session.GetId(), false);
            }
        }
        else if (ImGui::MenuItem("Pin Tab"))
        {
            SetSessionPinned(session.GetId(), true);
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Save"))
        {
            const EditorDocumentTypeInfo* typeInfo = m_TypeRegistry.Find(session.GetTypeId());
            if (typeInfo != nullptr && typeInfo->SaveSession && m_Context != nullptr)
            {
                if (typeInfo->SaveSession(*m_Context, session))
                {
                    session.SetDirty(false);
                }
            }
        }

        if (m_Context != nullptr)
        {
            const EditorDocumentTypeInfo* typeInfo = m_TypeRegistry.Find(session.GetTypeId());
            if (typeInfo != nullptr && typeInfo->AppendTabContextMenu)
            {
                ImGui::Separator();
                typeInfo->AppendTabContextMenu(*m_Context, session);
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Copy Asset Path", nullptr, false, !session.GetAssetKey().empty()))
        {
            CopyAssetPath(session.GetId(), false);
        }
        if (ImGui::MenuItem("Copy Relative Path", nullptr, false, !session.GetAssetKey().empty()))
        {
            CopyAssetPath(session.GetId(), true);
        }
        if (ImGui::MenuItem("Show in Content Browser") && m_Context != nullptr)
        {
            m_Context->GetAssetWorkflow().RevealAssetInContentBrowser(session.GetAssetKey());
        }
        if (ImGui::MenuItem("Reveal in Explorer", nullptr, false, !session.GetAssetKey().empty()))
        {
            RevealInOsExplorer(session.GetId());
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Reopen Closed Tab", "Ctrl+Shift+T", false, CanReopenClosedTab()))
        {
            ReopenClosedTab();
        }
    }
}
