#include "Shell/Document/EditorDocumentHost.h"

#include "Shell/IEditorContext.h"
#include "Services/AssetWorkflowModule.h"
#include "Runtime/Resource/AssetMeta.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Core/Log/LogSystem.h"

#include "imgui.h"

#include <algorithm>
#include <filesystem>

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
        if (index < m_Sessions.size())
        {
            m_Sessions.erase(m_Sessions.begin() + static_cast<std::ptrdiff_t>(index));
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

    void EditorDocumentHost::Reorder(size_t fromIndex, size_t toIndex)
    {
        if (fromIndex >= m_Sessions.size() || toIndex >= m_Sessions.size() || fromIndex == toIndex)
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
                    const bool selected = ImGui::BeginTabItem(session->GetTitle().c_str(), &open, flags);

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
                        if (ImGui::MenuItem("Close"))
                        {
                            RequestClose(session->GetId());
                        }
                        if (ImGui::MenuItem("Close Others", nullptr, false, m_Sessions.size() > 1))
                        {
                            RequestCloseOthers(session->GetId());
                        }
                        if (ImGui::MenuItem("Close to the Right", nullptr, false, i + 1 < m_Sessions.size()))
                        {
                            RequestCloseToTheRight(session->GetId());
                        }
                        if (ImGui::MenuItem("Close Saved"))
                        {
                            RequestCloseSaved();
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Save"))
                        {
                            const EditorDocumentTypeInfo* typeInfo = m_TypeRegistry.Find(session->GetTypeId());
                            if (typeInfo != nullptr && typeInfo->SaveSession && m_Context != nullptr)
                            {
                                if (typeInfo->SaveSession(*m_Context, *session))
                                {
                                    session->SetDirty(false);
                                }
                            }
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Copy Asset Path"))
                        {
                            ImGui::SetClipboardText(session->GetAssetKey().c_str());
                        }
                        if (ImGui::MenuItem("Show in Content Browser") && m_Context != nullptr)
                        {
                            m_Context->GetAssetWorkflow().RevealAssetInContentBrowser(session->GetAssetKey());
                        }
                        ImGui::EndPopup();
                    }

                    if (selected)
                    {
                        uiSelectedId = session->GetId();
                        ImGui::EndTabItem();
                    }
                    else if (!open)
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
}
